/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: QR codes, for a phone to open a link the console shows (Report on
 * GitHub). Project Nayuki's QR Code generator (third_party/qrcodegen,
 * unchanged, MIT) makes them. */
#ifndef PC5_QR_H
#define PC5_QR_H
#include "pc5.h"

enum { PC5_QR_BYTES = 3918 };   /* qrcodegen_BUFFER_LEN_MAX: a version 40 code */

typedef struct Pc5Qr {
    int size;                   /* modules a side, 0 when there is none */
    uint8_t code[PC5_QR_BYTES];
} Pc5Qr;

/* The smallest code that holds text, with the most error correction that
 * keeps it that size. 0, or -1 when text doesn't fit any. */
int pc5_qr_make(const char *text, Pc5Qr *qr);
/* 1 when the module at x, y (from the top left, 0 to size - 1) is dark. */
int pc5_qr_dark(const Pc5Qr *qr, int x, int y);

#endif
