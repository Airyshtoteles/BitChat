/**
 * @file storage_sqlite.c
 * @brief SQLite-backed storage adapter for store-and-forward persistence.
 *
 * Implements the bc_storage_t interface using SQLite3. Messages are stored
 * with their full serialized form so they survive app restarts. Prepared
 * statements are used for performance.
 */

#include "bitchat/port/storage.h"
#include "bitchat/error.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Private context for the SQLite storage adapter. */
typedef struct sqlite_ctx {
    sqlite3      *db;
    sqlite3_stmt *stmt_insert;
    sqlite3_stmt *stmt_get_pending;
    sqlite3_stmt *stmt_delete;
    sqlite3_stmt *stmt_evict;
} sqlite_ctx_t;

static const char *SQL_CREATE_TABLE =
    "CREATE TABLE IF NOT EXISTS messages ("
    "  uuid      BLOB PRIMARY KEY,"
    "  sender    BLOB NOT NULL,"
    "  receiver  BLOB NOT NULL,"
    "  payload   BLOB,"
    "  ttl       INTEGER NOT NULL,"
    "  timestamp INTEGER NOT NULL"
    ");";

static const char *SQL_INSERT =
    "INSERT OR REPLACE INTO messages (uuid, sender, receiver, payload, ttl, timestamp) "
    "VALUES (?, ?, ?, ?, ?, ?);";

static const char *SQL_GET_PENDING =
    "SELECT uuid, sender, receiver, payload, ttl, timestamp "
    "FROM messages WHERE receiver = ?;";

static const char *SQL_DELETE =
    "DELETE FROM messages WHERE uuid = ?;";

static const char *SQL_EVICT =
    "DELETE FROM messages WHERE timestamp < ?;";

/* ---------- vtable implementations ---------- */

static int sqlite_store_message(bc_storage_t *self, const bc_message_t *msg)
{
    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_insert);
    sqlite3_bind_blob(ctx->stmt_insert, 1, msg->uuid, BC_UUID_SIZE, SQLITE_STATIC);
    sqlite3_bind_blob(ctx->stmt_insert, 2, msg->sender_pubkey, BC_PUBKEY_SIZE, SQLITE_STATIC);
    sqlite3_bind_blob(ctx->stmt_insert, 3, msg->receiver_pubkey, BC_PUBKEY_SIZE, SQLITE_STATIC);
    sqlite3_bind_blob(ctx->stmt_insert, 4, msg->payload, (int)msg->payload_len, SQLITE_STATIC);
    sqlite3_bind_int(ctx->stmt_insert, 5, msg->ttl);
    sqlite3_bind_int64(ctx->stmt_insert, 6, (sqlite3_int64)msg->timestamp);

    int rc = sqlite3_step(ctx->stmt_insert);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

static int sqlite_get_pending(bc_storage_t *self,
                              const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                              bc_message_t ***msgs_out, size_t *count)
{
    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_get_pending);
    sqlite3_bind_blob(ctx->stmt_get_pending, 1, peer_pubkey, BC_PUBKEY_SIZE, SQLITE_STATIC);

    /* First pass: count results. */
    size_t n = 0;
    while (sqlite3_step(ctx->stmt_get_pending) == SQLITE_ROW) {
        n++;
    }

    if (n == 0) {
        *msgs_out = NULL;
        *count = 0;
        return BC_OK;
    }

    /* Allocate output array. */
    bc_message_t **arr = calloc(n, sizeof(bc_message_t *));
    if (!arr) {
        return BC_ERR_NOMEM;
    }

    /* Second pass: read rows. */
    sqlite3_reset(ctx->stmt_get_pending);
    sqlite3_bind_blob(ctx->stmt_get_pending, 1, peer_pubkey, BC_PUBKEY_SIZE, SQLITE_STATIC);

    size_t idx = 0;
    while (sqlite3_step(ctx->stmt_get_pending) == SQLITE_ROW && idx < n) {
        bc_message_t *msg = calloc(1, sizeof(*msg));
        if (!msg) {
            /* Clean up already-allocated messages. */
            for (size_t i = 0; i < idx; i++) {
                bc_message_destroy(arr[i]);
            }
            free(arr);
            return BC_ERR_NOMEM;
        }

        const void *uuid_blob = sqlite3_column_blob(ctx->stmt_get_pending, 0);
        const void *sender_blob = sqlite3_column_blob(ctx->stmt_get_pending, 1);
        const void *receiver_blob = sqlite3_column_blob(ctx->stmt_get_pending, 2);
        const void *payload_blob = sqlite3_column_blob(ctx->stmt_get_pending, 3);
        int payload_len = sqlite3_column_bytes(ctx->stmt_get_pending, 3);

        if (uuid_blob) memcpy(msg->uuid, uuid_blob, BC_UUID_SIZE);
        if (sender_blob) memcpy(msg->sender_pubkey, sender_blob, BC_PUBKEY_SIZE);
        if (receiver_blob) memcpy(msg->receiver_pubkey, receiver_blob, BC_PUBKEY_SIZE);

        msg->ttl = (uint8_t)sqlite3_column_int(ctx->stmt_get_pending, 4);
        msg->timestamp = (uint64_t)sqlite3_column_int64(ctx->stmt_get_pending, 5);

        if (payload_blob && payload_len > 0) {
            msg->payload = malloc((size_t)payload_len);
            if (!msg->payload) {
                free(msg);
                for (size_t i = 0; i < idx; i++) {
                    bc_message_destroy(arr[i]);
                }
                free(arr);
                return BC_ERR_NOMEM;
            }
            memcpy(msg->payload, payload_blob, (size_t)payload_len);
            msg->payload_len = (size_t)payload_len;
        }

        arr[idx++] = msg;
    }

    *msgs_out = arr;
    *count = idx;
    return BC_OK;
}

static int sqlite_delete_message(bc_storage_t *self,
                                 const uint8_t uuid[BC_UUID_SIZE])
{
    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    sqlite3_reset(ctx->stmt_delete);
    sqlite3_bind_blob(ctx->stmt_delete, 1, uuid, BC_UUID_SIZE, SQLITE_STATIC);

    int rc = sqlite3_step(ctx->stmt_delete);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

static int sqlite_evict_expired(bc_storage_t *self, uint64_t max_age_sec)
{
    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;

    uint64_t cutoff = (uint64_t)time(NULL) - max_age_sec;

    sqlite3_reset(ctx->stmt_evict);
    sqlite3_bind_int64(ctx->stmt_evict, 1, (sqlite3_int64)cutoff);

    int rc = sqlite3_step(ctx->stmt_evict);
    return (rc == SQLITE_DONE) ? BC_OK : BC_ERR_STORAGE;
}

static void sqlite_destroy(bc_storage_t *self)
{
    if (!self) {
        return;
    }

    sqlite_ctx_t *ctx = (sqlite_ctx_t *)self->ctx;
    if (ctx) {
        if (ctx->stmt_insert) sqlite3_finalize(ctx->stmt_insert);
        if (ctx->stmt_get_pending) sqlite3_finalize(ctx->stmt_get_pending);
        if (ctx->stmt_delete) sqlite3_finalize(ctx->stmt_delete);
        if (ctx->stmt_evict) sqlite3_finalize(ctx->stmt_evict);
        if (ctx->db) sqlite3_close(ctx->db);
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

    if (sqlite3_open(db_path, &ctx->db) != SQLITE_OK) {
        free(ctx);
        return NULL;
    }

    /* Create table. */
    char *err_msg = NULL;
    if (sqlite3_exec(ctx->db, SQL_CREATE_TABLE, NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    /* Prepare statements. */
    int ok = 1;
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_INSERT, -1, &ctx->stmt_insert, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_GET_PENDING, -1, &ctx->stmt_get_pending, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_DELETE, -1, &ctx->stmt_delete, NULL) == SQLITE_OK);
    ok = ok && (sqlite3_prepare_v2(ctx->db, SQL_EVICT, -1, &ctx->stmt_evict, NULL) == SQLITE_OK);

    if (!ok) {
        if (ctx->stmt_insert) sqlite3_finalize(ctx->stmt_insert);
        if (ctx->stmt_get_pending) sqlite3_finalize(ctx->stmt_get_pending);
        if (ctx->stmt_delete) sqlite3_finalize(ctx->stmt_delete);
        if (ctx->stmt_evict) sqlite3_finalize(ctx->stmt_evict);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    /* Build the vtable. */
    bc_storage_t *storage = calloc(1, sizeof(*storage));
    if (!storage) {
        sqlite3_finalize(ctx->stmt_insert);
        sqlite3_finalize(ctx->stmt_get_pending);
        sqlite3_finalize(ctx->stmt_delete);
        sqlite3_finalize(ctx->stmt_evict);
        sqlite3_close(ctx->db);
        free(ctx);
        return NULL;
    }

    storage->store_message = sqlite_store_message;
    storage->get_pending   = sqlite_get_pending;
    storage->delete_message = sqlite_delete_message;
    storage->evict_expired = sqlite_evict_expired;
    storage->destroy       = sqlite_destroy;
    storage->ctx           = ctx;

    return storage;
}
