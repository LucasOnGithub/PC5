/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5's QR codes (pc5_qr.h) with Project Nayuki's QR Code generator
 * (third_party/qrcodegen, unchanged, MIT, from github.com/nayuki/QR-Code-generator).
 * Its own warnings are not PC5's to fix, so they are switched off for this
 * file alone; its checks of its own arguments are left out (NDEBUG). */
#include "pc5_qr.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#define NDEBUG 1
#include "third_party/qrcodegen/qrcodegen.c"

_Static_assert(PC5_QR_BYTES == qrcodegen_BUFFER_LEN_MAX, "pc5_qr.h's size is qrcodegen's");

int pc5_qr_make(const char *text, Pc5Qr *qr)
{
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    qr->size = 0;
    if (!qrcodegen_encodeText(text, temp, qr->code, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                              qrcodegen_Mask_AUTO, true))
        return -1;
    qr->size = qrcodegen_getSize(qr->code);
    return 0;
}

int pc5_qr_dark(const Pc5Qr *qr, int x, int y)
{
    return qr->size && qrcodegen_getModule(qr->code, x, y);
}
