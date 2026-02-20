/**
 * @file error.h
 * @brief Common error codes for BitChat.
 */

#ifndef BITCHAT_ERROR_H
#define BITCHAT_ERROR_H

/** Operation succeeded. */
#define BC_OK                   0
/** Generic failure. */
#define BC_ERR                 -1
/** Out of memory. */
#define BC_ERR_NOMEM           -2
/** Invalid argument. */
#define BC_ERR_INVALID         -3
/** Duplicate message (already seen). */
#define BC_ERR_DUPLICATE       -4
/** TTL expired. */
#define BC_ERR_TTL_EXPIRED     -5
/** Cryptographic operation failed. */
#define BC_ERR_CRYPTO          -6
/** Storage operation failed. */
#define BC_ERR_STORAGE         -7
/** Transport operation failed. */
#define BC_ERR_TRANSPORT       -8
/** Feature not implemented. */
#define BC_ERR_NOT_IMPLEMENTED -9
/** Buffer too small. */
#define BC_ERR_BUFFER_TOO_SMALL -10

#endif /* BITCHAT_ERROR_H */
