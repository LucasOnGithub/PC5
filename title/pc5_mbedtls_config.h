/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5's changes to mbedTLS 3.6's default configuration (its
 * MBEDTLS_USER_CONFIG_FILE). PC5 brings its own sockets, files, random bytes
 * and calendar (pc5_net.c, pc5_net_ps5.c); the rest is mbedTLS as it comes. */
#ifndef PC5_MBEDTLS_CONFIG_H
#define PC5_MBEDTLS_CONFIG_H

#undef MBEDTLS_NET_C                /* BSD sockets: PC5 has its own */
#undef MBEDTLS_TIMING_C             /* alarm() and signals */
#undef MBEDTLS_FS_IO                /* PC5 reads the certificates itself */
#undef MBEDTLS_PSA_ITS_FILE_C       /* needs MBEDTLS_FS_IO */
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C /* needs the above */
#undef MBEDTLS_SELF_TEST
#undef MBEDTLS_DEBUG_C
#define MBEDTLS_NO_PLATFORM_ENTROPY    /* no getrandom or /dev/urandom on the console: */
#define MBEDTLS_ENTROPY_HARDWARE_ALT   /* mbedtls_hardware_poll, from the system's generator */
#define MBEDTLS_PLATFORM_GMTIME_R_ALT  /* mbedtls_platform_gmtime_r, PC5's own */
#define MBEDTLS_PLATFORM_MEMORY        /* its memory from pc5_alloc, set by pc5_net.c */

#endif
