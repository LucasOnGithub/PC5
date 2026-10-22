/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* stb_image (public domain, third_party/stb_image.h) built for PC5: JPEG and
 * PNG only, from memory, with PC5's allocator. Its own warnings are not
 * PC5's to fix, so they are switched off for this file alone. */
#include "pc5.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS   /* the console title has no emulated TLS */
#define STBI_MALLOC(size) pc5_alloc(size)
#define STBI_REALLOC(block, size) pc5_realloc(block, size)
#define STBI_FREE(block) pc5_free(block)
#define STBI_ASSERT(x) ((void)0)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
