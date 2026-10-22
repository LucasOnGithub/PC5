/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: HTTPS GET (pc5_net.h), on mbedTLS 3.6 (Apache-2.0), configured by
 * pc5_mbedtls_config.h. */
#include "pc5_net.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { CONNECT_MS = 8000, IO_MS = 15000, REDIRECTS = 4, HEADER_MAX = 64 * 1024 };

static Pc5NetOps ops;
static int have_ops, crypto_state;     /* crypto_state: 0 not tried, 1 ready, -1 failed */
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;
static mbedtls_x509_crt roots;

#define USER_AGENT "PC5/" PC5_VERSION " (PS5 homebrew game launcher; +https://github.com/bornaradusin/PC5)"

static void note(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void note(const char *format, ...)
{
    char line[600];
    va_list args;
    if (!have_ops || !ops.log) return;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    ops.log(line);
}

void pc5_net_init(const Pc5NetOps *platform)
{
    ops = *platform;
    have_ops = 1;
}

/* ---- what mbedTLS asks of the platform (pc5_mbedtls_config.h) ---------------------------- */

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t length, size_t *written);
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t length, size_t *written)
{
    (void)data;
    *written = 0;
    if (!have_ops || !ops.random || ops.random(output, length)) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *written = length;
    return 0;
}

/* UTC calendar time, for certificate dates (Howard Hinnant's civil_from_days). */
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *tm)
{
    long long t = (long long)*tt, days = t / 86400, rest = t % 86400;
    if (rest < 0) { rest += 86400; days--; }
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    long long d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    memset(tm, 0, sizeof(*tm));
    tm->tm_year = (int)(y + (m <= 2) - 1900);
    tm->tm_mon = (int)m - 1;
    tm->tm_mday = (int)d;
    tm->tm_hour = (int)(rest / 3600);
    tm->tm_min = (int)(rest % 3600 / 60);
    tm->tm_sec = (int)(rest % 60);
    tm->tm_wday = (int)((days % 7 + 11) % 7);   /* 1970-01-01 was a Thursday */
    tm->tm_yday = (int)(doy >= 306 ? doy - 306 : doy + 59 + ((y % 4 == 0 && y % 100) || y % 400 == 0));
    return tm;
}

/* The random generator and the console's roots, once. */
/* mbedTLS's memory from PC5's allocator, where a big block gets pages of its
 * own instead of the title's heap. */
static void *tls_calloc(size_t count, size_t size)
{
    return count && size > SIZE_MAX / count ? NULL : pc5_alloc(count * size);
}

static int crypto_ready(void)
{
    if (crypto_state) return crypto_state > 0;
    crypto_state = -1;
    mbedtls_platform_set_calloc_free(tls_calloc, pc5_free);
    int status = (int)psa_crypto_init();
    if (status) { note("PC5 net psa_crypto_init=%d", status); return 0; }
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    if ((status = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"pc5", 3))) {
        note("PC5 net random seed=-%#x", (unsigned)-status);
        return 0;
    }
    mbedtls_x509_crt_init(&roots);
    /* Every file there is, each adding its roots to the others'. */
    int parsed = 0;
    for (const char *const *file = ops.ca_files; file && *file; file++) {
        Pc5Buf pem = { 0 };
        if (!pc5_read_file(*file, &pem, 4 * 1024 * 1024) && !pc5_buf_terminate(&pem)) {
            /* PEM wants its closing 0 counted. */
            status = mbedtls_x509_crt_parse(&roots, pem.data, pem.len + 1);
            int count = 0;
            for (const mbedtls_x509_crt *c = &roots; c && c->version; c = c->next) count++;
            note("PC5 net roots file=%s certificates=%d refused=%d", *file, count - parsed, status > 0 ? status : 0);
            parsed = count;
        } else {
            note("PC5 net roots file=%s unreadable", *file);
        }
        pc5_buf_free(&pem);
    }
    if (!parsed) return 0;
    crypto_state = 1;
    return 1;
}

/* ---- the socket under TLS -------------------------------------------------------------------- */

static int bio_send(void *context, const unsigned char *data, size_t size)
{
    int got = ops.send(*(int *)context, data, size, IO_MS);
    return got > 0 ? got : -0x004E;   /* MBEDTLS_ERR_NET_SEND_FAILED */
}

static int bio_recv(void *context, unsigned char *data, size_t size)
{
    int got = ops.recv(*(int *)context, data, size, IO_MS);
    return got >= 0 ? got : -0x004C;  /* MBEDTLS_ERR_NET_RECV_FAILED */
}

int pc5_url_split(const char *url, char *host, size_t host_size, uint16_t *port, char *path, size_t path_size)
{
    const char *at, *slash, *colon;
    size_t host_length;

    if (strncmp(url, "https://", 8)) return -1;
    at = url + 8;
    slash = strchr(at, '/');
    if (!slash) slash = at + strlen(at);
    colon = memchr(at, ':', (size_t)(slash - at));
    host_length = (size_t)((colon ? colon : slash) - at);
    if (!host_length || host_length >= host_size) return -1;
    memcpy(host, at, host_length);
    host[host_length] = 0;
    *port = 443;
    if (colon) {
        long value = strtol(colon + 1, NULL, 10);
        if (value <= 0 || value > 65535) return -1;
        *port = (uint16_t)value;
    }
    return pc5_format(path, path_size, "%s", *slash ? slash : "/");
}

/* One request: the raw answer (headers and body) into answer. */
static int exchange(const char *host, uint16_t port, const char *path, Pc5Buf *answer, size_t max)
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    uint32_t address = 0;
    int sock = -1, status = -1, r;
    char request[2600];

    if (ops.resolve(host, &address)) {
        note("PC5 net no address for %s", host);
        return -1;
    }
    if ((sock = ops.connect(address, port, CONNECT_MS)) < 0) {
        note("PC5 net can't connect to %s:%u", host, (unsigned)port);
        return -1;
    }
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    if ((r = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT)))
        goto out;
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &roots, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
    if ((r = mbedtls_ssl_setup(&ssl, &conf)) || (r = mbedtls_ssl_set_hostname(&ssl, host))) goto out;
    mbedtls_ssl_set_bio(&ssl, &sock, bio_send, bio_recv, NULL);
    while ((r = mbedtls_ssl_handshake(&ssl))) {
        if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
            char why[120];
            uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
            mbedtls_strerror(r, why, sizeof(why));
            note("PC5 net handshake with %s failed: -%#x %s (verify %#x)", host, (unsigned)-r, why, (unsigned)flags);
            goto out;
        }
    }
    if (pc5_format(request, sizeof(request),
                   "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " USER_AGENT "\r\nAccept: */*\r\n"
                   "Accept-Encoding: identity\r\nConnection: close\r\n\r\n", path, host))
        goto out;
    for (size_t done = 0, length = strlen(request); done < length;) {
        r = mbedtls_ssl_write(&ssl, (const unsigned char *)request + done, length - done);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r <= 0) goto out;
        done += (size_t)r;
    }
    answer->len = 0;
    for (;;) {
        if (pc5_buf_reserve(answer, 32 * 1024)) goto out;
        r = mbedtls_ssl_read(&ssl, answer->data + answer->len, 32 * 1024);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
        if (r == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;
#endif
        if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == 0) break;
        if (r < 0) {
            /* Some servers close without notifying: what came is what there is. */
            if (answer->len) break;
            goto out;
        }
        answer->len += (size_t)r;
        if (answer->len > max + HEADER_MAX) goto out;
    }
    status = 0;
    mbedtls_ssl_close_notify(&ssl);
out:
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    ops.close(sock);
    return status;
}

static int header_value(const char *headers, const char *name, char *out, size_t size)
{
    size_t length = strlen(name);
    for (const char *line = strstr(headers, "\r\n"); line && line[2]; line = strstr(line + 2, "\r\n")) {
        const char *p = line + 2;
        size_t k = 0;
        while (k < length && p[k] && pc5_lower((unsigned char)p[k]) == pc5_lower((unsigned char)name[k])) k++;
        if (k == length && p[k] == ':') {
            p += k + 1;
            while (*p == ' ' || *p == '\t') p++;
            size_t n = 0;
            while (p[n] && p[n] != '\r' && n + 1 < size) { out[n] = p[n]; n++; }
            out[n] = 0;
            return 0;
        }
    }
    return -1;
}

/* The body of a raw answer, in place: chunks joined, or cut to its length. */
static int take_body(Pc5Buf *answer, size_t header_length, const char *headers)
{
    char value[64];
    uint8_t *body = answer->data + header_length;
    size_t length = answer->len - header_length;

    if (!header_value(headers, "Transfer-Encoding", value, sizeof(value)) && strstr(value, "chunked")) {
        size_t in = 0, out = 0;
        for (;;) {
            char *end;
            if (in >= length) return -1;
            unsigned long size = strtoul((char *)body + in, &end, 16);
            const char *eol = strstr((char *)body + in, "\r\n");
            if (!eol || end == (char *)body + in) return -1;
            in = (size_t)((const uint8_t *)eol - body) + 2;
            if (!size) break;
            if (size > length - in) return -1;
            memmove(answer->data + out, body + in, size);
            out += size;
            in += size + 2;
        }
        answer->len = out;
    } else {
        if (!header_value(headers, "Content-Length", value, sizeof(value))) {
            size_t declared = (size_t)strtoull(value, NULL, 10);
            if (declared > length) return -1;   /* cut short */
            length = declared;
        }
        memmove(answer->data, body, length);
        answer->len = length;
    }
    return pc5_buf_terminate(answer);
}

int pc5_net_get(void *context, const char *url, Pc5Buf *out, size_t max, int *status)
{
    char host[256], path[2048], next[2400], location[2048];
    uint16_t port;
    (void)context;

    out->len = 0;
    *status = 0;
    if (!have_ops || !crypto_ready()) return -1;
    if (pc5_format(next, sizeof(next), "%s", url)) return -1;
    for (int hop = 0; hop <= REDIRECTS; hop++) {
        if (pc5_url_split(next, host, sizeof(host), &port, path, sizeof(path))) {
            note("PC5 net not an https address: %.200s", next);
            return -1;
        }
        if (exchange(host, port, path, out, max)) return -1;
        if (pc5_buf_terminate(out)) return -1;
        char *end = strstr((char *)out->data, "\r\n\r\n");
        if (!end || strncmp((char *)out->data, "HTTP/1.", 7) || out->len < 12) {
            note("PC5 net %s answered with no HTTP headers", host);
            return -1;
        }
        *end = 0;
        size_t header_length = (size_t)(end - (char *)out->data) + 4;
        *status = atoi((char *)out->data + 9);
        if ((*status == 301 || *status == 302 || *status == 303 || *status == 307 || *status == 308) &&
            !header_value((char *)out->data, "Location", location, sizeof(location))) {
            if (!strncmp(location, "https://", 8)) pc5_format(next, sizeof(next), "%s", location);
            else if (!strncmp(location, "http://", 7)) pc5_format(next, sizeof(next), "https://%s", location + 7);
            else if (!strncmp(location, "//", 2)) pc5_format(next, sizeof(next), "https:%s", location);
            else if (location[0] == '/') pc5_format(next, sizeof(next), "https://%s:%u%s", host, (unsigned)port, location);
            else {
                char *slash = strrchr(path, '/');
                if (slash) slash[1] = 0;
                pc5_format(next, sizeof(next), "https://%s:%u%s%s", host, (unsigned)port, path, location);
            }
            continue;
        }
        if (take_body(out, header_length, (char *)out->data)) {
            note("PC5 net %s: the answer was cut short", host);
            return -1;
        }
        if (out->len > max) return -1;
        return 0;
    }
    note("PC5 net too many redirects from %.200s", url);
    return -1;
}
