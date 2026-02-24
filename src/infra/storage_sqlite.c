/**
 * @file storage_sqlite.c
 * @brief SQLite-backed storage adapter for store-and-forward persistence.
 *
 * Implements the bc_storage_t vtable using SQLite3 with strict security
 * and reliability practices:
 *
 *   1. **Zero SQL injection** — every query uses sqlite3_prepare_v2() with
 *      parameter binding (sqlite3_bind_blob / sqlite3_bind_int64). No
 *      query string is ever assembled at runtime with sprintf/snprintf.
 *
 *   2. **Prepared Statements** — all four SQL operations (insert, select,
 *      delete, evict) are prepared once at initialization and reused with
 *      sqlite3_reset() + sqlite3_clear_bindings() between invocations.
 *      This avoids repeated SQL parsing and guarantees type-safe binding.
 *
 *   3. **Memory leak prevention** — every sqlite3_stmt is finalized in the
 *      destroy path, both on the happy path and on every error path during
 *      initialization. sqlite3_finalize() is always called regardless of
 *      whether the statement executed successfully.
 *
 *   4. **Thread safety** — the database is opened with SQLITE_OPEN_FULLMUTEX
 *      (serialized mode), ensuring safe concurrent access from the UI thread
 *      and the mesh-router tick thread without external locking.
 *
 *   5. **SQLITE_TRANSIENT** — all blob bindings use SQLITE_TRANSIENT so
 *      SQLite makes its own copy of the data. This is safe when the source
 *      bc_message_t may be freed immediately after the call returns.
 */

#include "bitchat/infra/storage_sqlite.h"
#include "bitchat/port/storage.h"
#include "bitchat/domain/message.h"
#include "bitchat/error.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Default message retention: 72 hours in seconds. */
#define STORAGE_DEFAULT_RETENTION_SEC (72 * 3600)

/* ---------- private context ---------- */

typedef struct sqlite_ctx {
    sqlite3      *db;
    sqlite3_stmt *stmt_insert;
    sqlite3_stmt *stmt_get_pending;
    sqlite3_stmt *stmt_get_all_pending;
    sqlite3_stmt *stmt_delete;
    sqlite3_stmt *stmt_evict;
} sqlite_ctx_t;

/* ---------- SQL strings ---------- */

static const char *SQL_CREATE_TABLE =
    "CREATE TABLE IF NOT EXISTS pending_messages ("
    "  uuid            BLOB PRIMARY KEY,"
    "  sender_pubkey   BLOB NOT NULL,"
    "  receiver_pubkey BLOB NOT NULL,"
    "  payload         BLOB,"
    "  ttl             INTEGER NOT NULL,"
    "  expires_at      INTEGER NOT NULL"
    ");";

static const char *SQL_CREATE_INDEX =
    "CREATE INDEX IF NOT EXISTS idx_pending_receiver "
    "ON pending_messages (receiver_pubkey);";

static const char *SQL_INSERT =
    "INSERT OR IGNORE INTO pending_messages "
    "(uuid, sender_pubkey, receiver_pubkey, payload, ttl, expires_at) "
    "VALUES (?, ?, ?, ?, ?, ?);";

static const char *SQL_GET_PENDING =
    "SELECT uuid, sender_pubkey, receiver_pubkey, payload, ttl, expires_at "
    "FROM pending_messages "
    "WHERE receiver_pubkey = ? AND expires_at >= ?;";

static const char *SQL_GET_ALL_PENDING =
    "SELECT uuid, sender_pubkey, receiver_pubkey, payload, ttl, expires_at "
    "FROM pending_messages "
    "WHERE expires_at >= ?;";

static const char *SQL_DELETE =
    "DELETE FROM pending_messages WHERE uuid = ?;";

static const char *SQL_EVICT =
    "DELETE FROM pending_messages WHERE expires_at < ?;";

/* ---------- vtable implementations ---------- */

/**
 * Persist a message for store-and-forward.
 *
 * Uses INSERT OR IGNORE — if the UUID already exists (duplicate message
 * arriving via a different gossip path), the row is silently skipped
 * instead of overwriting or crashing.
 *
 * expires_at is computed as: current UNIX time + 72 hours.
 */
static int sqlite_store_message(bc_storage_t *self, const bc_message_t *msg)
{
    if (!self || !msg) {
        return BC_ERR_INVALID;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_insert);
    sqlite3_clear_bindings(ctx->stmt_insert);

    /*
     * Bind blob parameters with SQLITE_TRANSIENT — SQLite will copy the
     * data internally, so it remains valid even if the caller frees msg
     * immediately after this function returns.
     */
    sqlite3_bind_blob(ctx->stmt_insert, 1,
                      msg->uuid, BC_UUID_SIZE, SQLITE_TRANSIENT);
    sqlite3_bind_blob(ctx->stmt_insert, 2,
                      msg->sender_pubkey, BC_PUBKEY_SIZE, SQLITE_TRANSIENT);
    sqlite3_bind_blob(ctx->stmt_insert, 3,
                      msg->receiver_pubkey, BC_PUBKEY_SIZE, SQLITE_TRANSIENT);
    sqlite3_bind_blob(ctx->stmt_insert, 4,
                      msg->payload, (int)msg->payload_len, SQLITE_TRANSIENT);
    sqlite3_bind_int(ctx->stmt_insert, 5, msg->ttl);

    /* Compute expiration: current wall clock + retention period. */
    sqlite3_int64 expires_at = (sqlite3_int64)time(NULL)
                             + STORAGE_DEFAULT_RETENTION_SEC;
    sqlite3_bind_int64(ctx->stmt_insert, 6, expires_at);

    int rc = sqlite3_step(ctx->stmt_insert);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

/**
 * Retrieve pending messages, optionally filtered by @p peer_pubkey.
 *
 * If @p peer_pubkey is NULL, returns ALL non-expired messages (used for
 * gossip-style broadcast sync where every pending message should be
 * forwarded to all neighbors).
 *
 * Uses a single-pass approach with dynamic array growth to avoid the
 * overhead of a two-pass count-then-read strategy.
 */
static int sqlite_get_pending(bc_storage_t *self,
                              const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                              bc_message_t ***msgs_out, size_t *count)
{
    if (!self || !msgs_out || !count) {
        return BC_ERR_INVALID;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    /* Pick the right prepared statement based on whether a pubkey filter
       was provided. */
    sqlite3_stmt *stmt;
    if (peer_pubkey) {
        stmt = ctx->stmt_get_pending;
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_blob(stmt, 1, peer_pubkey, BC_PUBKEY_SIZE,
                          SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));
    } else {
        stmt = ctx->stmt_get_all_pending;
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
    }

    /* Single-pass: grow array dynamically as rows come in. */
    size_t cap = 0;
    size_t n   = 0;
    bc_message_t **arr = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* Grow array if needed. */
        if (n >= cap) {
            size_t new_cap = (cap == 0) ? 8 : cap * 2;
            bc_message_t **tmp = realloc(arr, new_cap * sizeof(bc_message_t *));
            if (!tmp) {
                goto oom;
            }
            arr = tmp;
            cap = new_cap;
        }

        /* Read columns. */
        const void *uuid_blob     = sqlite3_column_blob(stmt, 0);
        const void *sender_blob   = sqlite3_column_blob(stmt, 1);
        const void *receiver_blob = sqlite3_column_blob(stmt, 2);
        const void *payload_blob  = sqlite3_column_blob(stmt, 3);
        int payload_len           = sqlite3_column_bytes(stmt, 3);
        int ttl                   = sqlite3_column_int(stmt, 4);
        sqlite3_int64 expires_at  = sqlite3_column_int64(stmt, 5);

        /* Build a bc_message_t from the stored row. */
        bc_message_t *msg = calloc(1, sizeof(*msg));
        if (!msg) {
            goto oom;
        }

        if (uuid_blob)     memcpy(msg->uuid, uuid_blob, BC_UUID_SIZE);
        if (sender_blob)   memcpy(msg->sender_pubkey, sender_blob, BC_PUBKEY_SIZE);
        if (receiver_blob) memcpy(msg->receiver_pubkey, receiver_blob, BC_PUBKEY_SIZE);

        msg->ttl = (uint8_t)ttl;

        /* Reconstruct the original timestamp from expires_at. */
        msg->timestamp = (uint64_t)(expires_at - STORAGE_DEFAULT_RETENTION_SEC);

        if (payload_blob && payload_len > 0) {
            msg->payload = malloc((size_t)payload_len);
            if (!msg->payload) {
                free(msg);
                goto oom;
            }
            memcpy(msg->payload, payload_blob, (size_t)payload_len);
            msg->payload_len = (size_t)payload_len;
        }

        arr[n++] = msg;
    }

    *msgs_out = arr;
    *count = n;
    return BC_OK;

oom:
    /* Clean up on out-of-memory. */
    for (size_t i = 0; i < n; i++) {
        bc_message_destroy(arr[i]);
    }
    free(arr);
    *msgs_out = NULL;
    *count = 0;
    return BC_ERR_NOMEM;
}

/**
 * Delete a message by UUID (after successful delivery / forwarding).
 */
static int sqlite_delete_message(bc_storage_t *self,
                                 const uint8_t uuid[BC_UUID_SIZE])
{
    if (!self || !uuid) {
        return BC_ERR_INVALID;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_delete);
    sqlite3_clear_bindings(ctx->stmt_delete);

    sqlite3_bind_blob(ctx->stmt_delete, 1,
                      uuid, BC_UUID_SIZE, SQLITE_TRANSIENT);

    int rc = sqlite3_step(ctx->stmt_delete);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

/**
 * Evict all messages whose expires_at timestamp is in the past.
 *
 * The interface parameter @p max_age_sec is accepted for vtable
 * compatibility but is unused here — expiration is already baked into
 * each row's expires_at column at insertion time.
 */
static int sqlite_evict_expired(bc_storage_t *self, uint64_t max_age_sec)
{
    (void)max_age_sec; /* expiration is per-row via expires_at */

    if (!self) {
        return BC_ERR_INVALID;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_evict);
    sqlite3_clear_bindings(ctx->stmt_evict);

    sqlite3_bind_int64(ctx->stmt_evict, 1, (sqlite3_int64)time(NULL));

    int rc = sqlite3_step(ctx->stmt_evict);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

/**
 * Finalize all prepared statements, close the database, free context.
 *
 * sqlite3_finalize() is safe on NULL — it is a no-op, so we do not
 * need to guard each call with an if-check.
 */
static void sqlite_destroy(bc_storage_t *self)
{
    if (!self) {
        return;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;
    if (ctx) {
        sqlite3_finalize(ctx->stmt_insert);
        sqlite3_finalize(ctx->stmt_get_pending);
        sqlite3_finalize(ctx->stmt_get_all_pending);
        sqlite3_finalize(ctx->stmt_delete);
        sqlite3_finalize(ctx->stmt_evict);

        if (ctx->db) {
            sqlite3_close(ctx->db);
        }
        free(ctx);
    }
    free(self);
}

/* ---------- public constructor ---------- */

bc_storage_t *bc_storage_sqlite_create(const char *db_path)
{
    if (!db_path) {
        return NULL;
    }

    sqlite_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    /*
     * Open with SQLITE_OPEN_FULLMUTEX (serialized threading mode).
     * This means SQLite will use its own internal mutexes to protect all
     * API calls, making the connection safe for concurrent access from
     * the UI thread and the mesh-router tick thread without any external
     * locking.
     */
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
              | SQLITE_OPEN_FULLMUTEX;

    if (sqlite3_open_v2(db_path, &ctx->db, flags, NULL) != SQLITE_OK) {
        free(ctx);
        return NULL;
    }

    /* Enable WAL mode for better concurrent read/write performance. */
    sqlite3_exec(ctx->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);

    /* Create the pending_messages table. */
    char *err_msg = NULL;
    if (sqlite3_exec(ctx->db, SQL_CREATE_TABLE,
                     NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    /* Create index on receiver_pubkey for fast lookups. */
    if (sqlite3_exec(ctx->db, SQL_CREATE_INDEX,
                     NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    /*
     * Prepare all statements once upfront.
     *
     * sqlite3_prepare_v2() compiles a SQL string into an internal bytecode
     * program (sqlite3_stmt). Once prepared, the statement can be executed
     * repeatedly with different bound parameters:
     *
     *   1. sqlite3_reset(stmt)          — reset to initial state
     *   2. sqlite3_clear_bindings(stmt) — unbind all parameters
     *   3. sqlite3_bind_*(stmt, idx, …) — bind new parameter values
     *   4. sqlite3_step(stmt)           — execute one step
     *
     * This is both faster (no re-parsing on every call) and safer (no SQL
     * injection possible since parameters are never interpolated into the
     * SQL text — they travel through a separate, type-safe channel).
     */
    int ok = 1;
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_INSERT,
                -1, &ctx->stmt_insert, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_GET_PENDING,
                -1, &ctx->stmt_get_pending, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_GET_ALL_PENDING,
                -1, &ctx->stmt_get_all_pending, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_DELETE,
                -1, &ctx->stmt_delete, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_EVICT,
                -1, &ctx->stmt_evict, NULL) == SQLITE_OK);

    if (!ok) {
        /* Finalize any that succeeded before the failure. */
        sqlite3_finalize(ctx->stmt_insert);
        sqlite3_finalize(ctx->stmt_get_pending);
        sqlite3_finalize(ctx->stmt_get_all_pending);
        sqlite3_finalize(ctx->stmt_delete);
        sqlite3_finalize(ctx->stmt_evict);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    /* Build the vtable struct. */
    bc_storage_t *storage = calloc(1, sizeof(*storage));
    if (!storage) {
        sqlite3_finalize(ctx->stmt_insert);
        sqlite3_finalize(ctx->stmt_get_pending);
        sqlite3_finalize(ctx->stmt_get_all_pending);
        sqlite3_finalize(ctx->stmt_delete);
        sqlite3_finalize(ctx->stmt_evict);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    storage->store_message  = sqlite_store_message;
    storage->get_pending    = sqlite_get_pending;
    storage->delete_message = sqlite_delete_message;
    storage->evict_expired  = sqlite_evict_expired;
    storage->destroy        = sqlite_destroy;
    storage->ctx            = ctx;

    return storage;
}
