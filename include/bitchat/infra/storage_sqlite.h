/**
 * @file storage_sqlite.h
 * @brief SQLite-backed storage adapter — factory function declaration.
 *
 * This header exposes the constructor for the SQLite storage adapter.
 * It returns a fully wired @ref bc_storage_t vtable that the use-case
 * layer can consume without knowing the underlying engine.
 */

#ifndef BITCHAT_INFRA_STORAGE_SQLITE_H
#define BITCHAT_INFRA_STORAGE_SQLITE_H

#include "bitchat/port/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a SQLite-backed storage adapter.
 *
 * Opens (or creates) the database at @p db_path with serialized threading
 * mode (SQLITE_OPEN_FULLMUTEX) so it is safe to call from multiple threads.
 *
 * The returned @ref bc_storage_t owns all internal resources. Call its
 * @c destroy function pointer to close the database and free memory.
 *
 * @param db_path  Path to the SQLite database file.
 * @return Fully wired storage vtable, or NULL on failure.
 */
bc_storage_t *bc_storage_sqlite_create(const char *db_path);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_INFRA_STORAGE_SQLITE_H */
