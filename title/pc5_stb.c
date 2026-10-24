/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* stb_image and stb_image_write (public domain, third_party/) built for PC5:
 * JPEG and PNG read from memory, PNG written to memory, with PC5's allocator.
 * Their own warnings are not PC5's to fix, so they are switched off for this
 * file alone. */
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

/* stb_image_write (public domain, third_party/stb_image_write.h): PNG only, to
 * memory, with PC5's allocator, for the home screen tiles' pictures. */
#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(size) pc5_alloc(size)
#define STBIW_REALLOC(block, size) pc5_realloc(block, size)
#define STBIW_FREE(block) pc5_free(block)
#define STBIW_ASSERT(x) ((void)0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"
