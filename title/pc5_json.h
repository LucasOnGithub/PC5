/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: a small JSON reader for the answers of the cover sources. The text is
 * split into tokens once; values are then looked up by key and position. */
#ifndef PC5_JSON_H
#define PC5_JSON_H
#include "pc5.h"

typedef enum Pc5JsonType { PC5_JSON_NONE, PC5_JSON_OBJECT, PC5_JSON_ARRAY, PC5_JSON_STRING, PC5_JSON_NUMBER, PC5_JSON_LITERAL } Pc5JsonType;

typedef struct Pc5JsonToken {
    Pc5JsonType type;
    uint32_t start, end;     /* the text it covers (a string's without its quotes) */
    uint32_t children;       /* an object's keys plus values, an array's items */
    uint32_t next;           /* the token after it and everything inside it */
} Pc5JsonToken;

typedef struct Pc5Json {
    const char *text;
    Pc5JsonToken *tokens;
    uint32_t count;
} Pc5Json;

/* 0, or -1 for text that is not JSON (or too big: max_tokens). */
int pc5_json_parse(Pc5Json *json, const char *text, size_t length, uint32_t max_tokens);
void pc5_json_free(Pc5Json *json);
/* An object's value for key, an array's item; 0 when there is none (token 0
 * is the whole document, so it is never a child). */
uint32_t pc5_json_get(const Pc5Json *json, uint32_t object, const char *key);
uint32_t pc5_json_item(const Pc5Json *json, uint32_t array, uint32_t index);
/* A string's text, unescaped to UTF-8; a number's or literal's text as is. */
int pc5_json_text(const Pc5Json *json, uint32_t token, char *out, size_t size);
/* A number (or a string holding one); fallback when it is neither. */
double pc5_json_number(const Pc5Json *json, uint32_t token, double fallback);

#endif
