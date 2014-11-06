/* ==========================================================================
 * phf.h - Tiny perfect hash function library.
 * --------------------------------------------------------------------------
 * Copyright (c) 2014  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef PHF_H
#define PHF_H

#include <stddef.h>   /* size_t */
#include <stdint.h>   /* UINT32_MAX uint32_t uint64_t */
#include <stdbool.h>  /* bool */
#include <inttypes.h> /* PRIu32 PRIx32 */


struct phf {
	bool nodiv;

	uint32_t seed;

	size_t r; /* number of elements in g */
	size_t m; /* number of elements in perfect hash */
	uint32_t *g; /* displacement map indexed by g(k) % r */

	size_t d_max; /* maximum displacement value in g */
}; /* struct phf */


#define PHF_HASH_MAX UINT32_MAX
#define PHF_PRIuHASH PRIu32
#define PHF_PRIxHASH PRIx32

typedef uint32_t phf_hash_t;

typedef struct phf_string {
	void *p;
	size_t n;
} phf_string_t;


#endif /* PHF_H */
