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


/*
 * C O M P I L E R  F E A T U R E S  &  D I A G N O S T I C S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define PHF_GNUC_PREREQ(M, m) (__GNUC__ > (M) || (__GNUC__ == (M) && __GNUC_MINOR__ >= (m)))

#ifdef __clang__
#define phf_has_extension(x) __has_extension(x)
#define phf_has_attribute(x) __has_attribute(x)
#else
#define phf_has_extension(x) 0
#define phf_has_attribute(x) 0
#endif

#ifndef PHF_HAVE_NOEXCEPT
#define PHF_HAVE_NOEXCEPT \
	(__cplusplus >= 201103L || \
	 phf_has_extension(cxx_noexcept) || \
	 PHF_GNUC_PREREQ(4, 6))
#endif

#ifndef PHF_HAVE_GENERIC
#define PHF_HAVE_GENERIC \
	(__STDC_VERSION__ >= 201112L || \
	 phf_has_extension(c_generic_selections) || \
	 PHF_GNUC_PREREQ(4, 9))
#endif

#ifndef PHF_HAVE_BUILTIN_TYPES_COMPATIBLE_P
#define PHF_HAVE_BUILTIN_TYPES_COMPATIBLE_P (defined __GNUC__)
#endif

#ifndef PHF_HAVE_BUILTIN_CHOOSE_EXPR
#define PHF_HAVE_BUILTIN_CHOOSE_EXPR (defined __GNUC__)
#endif

#ifndef PHF_HAVE_ATTRIBUTE_VISIBILITY
#define PHF_HAVE_ATTRIBUTE_VISIBILITY \
	(phf_has_attribute(visibility) || PHF_GNUC_PREREQ(4, 0))
#endif

#ifdef __clang__
#pragma clang diagnostic push
#if __cplusplus < 201103L
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Wvariadic-macros"
#endif
#elif PHF_GNUC_PREREQ(4, 6)
#pragma GCC diagnostic push
#if __cplusplus < 201103L
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif
#endif


/*
 * C / C + +  V I S I B I L I T Y
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef PHF_PUBLIC
#define PHF_PUBLIC
#endif

#ifndef PHF_LOCAL
#if PHF_HAVE_ATTRIBUTE_VISIBILITY
#define PHF_LOCAL __attribute__((visibility("hidden")))
#else
#define PHF_LOCAL
#endif
#endif


/*
 * C / C + +  S H A R E D  T Y P E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define phf_error_t int /* for documentation purposes */

#define PHF_HASH_MAX UINT32_MAX
#define PHF_PRIuHASH PRIu32
#define PHF_PRIxHASH PRIx32

typedef uint32_t phf_hash_t;
typedef uint32_t phf_seed_t;

typedef struct phf_string {
	void *p;
	size_t n;
} phf_string_t;

struct phf {
	bool nodiv;

	phf_seed_t seed;

	size_t r; /* number of elements in g */
	size_t m; /* number of elements in perfect hash */
	uint32_t *g; /* displacement map indexed by g(k) % r */

	size_t d_max; /* maximum displacement value in g */

	enum {
		PHF_G_UINT8 = 1,
		PHF_G_UINT16,
		PHF_G_UINT32,
	} g_type;
}; /* struct phf */


/*
 * C + +  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef __cplusplus

#if !PHF_NO_LIBCXX
#include <string> /* std::string */
#endif

namespace PHF {
	template<typename key_t, bool nodiv>
	PHF_PUBLIC phf_error_t init(struct phf *, const key_t[], const size_t, const size_t, const size_t, const phf_seed_t);

	template<typename key_t>
	PHF_PUBLIC phf_hash_t hash(struct phf *, key_t);

	PHF_PUBLIC void destroy(struct phf *);

	static void compact(struct phf *);
}

extern template phf_error_t PHF::init<uint32_t, true>(struct phf *, const uint32_t[], const size_t, const size_t, const size_t, const phf_seed_t);
extern template phf_error_t PHF::init<uint64_t, true>(struct phf *, const uint64_t[], const size_t, const size_t, const size_t, const phf_seed_t);
extern template phf_error_t PHF::init<phf_string_t, true>(struct phf *, const phf_string_t[], const size_t, const size_t, const size_t, const phf_seed_t);
#if !PHF_NO_LIBCXX
extern template phf_error_t PHF::init<std::string, true>(struct phf *, const std::string[], const size_t, const size_t, const size_t, const phf_seed_t);
#endif

extern template phf_error_t PHF::init<uint32_t, false>(struct phf *, const uint32_t[], const size_t, const size_t, const size_t, const phf_seed_t);
extern template phf_error_t PHF::init<uint64_t, false>(struct phf *, const uint64_t[], const size_t, const size_t, const size_t, const phf_seed_t);
extern template phf_error_t PHF::init<phf_string_t, false>(struct phf *, const phf_string_t[], const size_t, const size_t, const size_t, const phf_seed_t);
#if !PHF_NO_LIBCXX
extern template phf_error_t PHF::init<std::string, false>(struct phf *, const std::string[], const size_t, const size_t, const size_t, const phf_seed_t);
#endif

extern template phf_hash_t PHF::hash<uint32_t>(struct phf *, uint32_t);
extern template phf_hash_t PHF::hash<uint64_t>(struct phf *, uint64_t);
extern template phf_hash_t PHF::hash<phf_string_t>(struct phf *, phf_string_t);
#if !PHF_NO_LIBCXX
extern template phf_hash_t PHF::hash<std::string>(struct phf *, std::string);
#endif

#endif /* __cplusplus */


/*
 * C 8 9  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef __cplusplus
extern "C" {
#endif

PHF_PUBLIC phf_error_t phf_init_uint32(struct phf *, uint32_t *, size_t, const size_t, const size_t, const phf_seed_t, bool nodiv);
PHF_PUBLIC phf_error_t phf_init_uint64(struct phf *, uint64_t *, size_t, const size_t, const size_t, const phf_seed_t, bool nodiv);
PHF_PUBLIC phf_error_t phf_init_string(struct phf *, phf_string_t *, size_t, const size_t, const size_t, const phf_seed_t, bool nodiv);

PHF_PUBLIC phf_hash_t phf_hash_uint32(struct phf *, uint32_t);
PHF_PUBLIC phf_hash_t phf_hash_uint64(struct phf *, uint64_t);
PHF_PUBLIC phf_hash_t phf_hash_string(struct phf *, phf_string_t);

PHF_PUBLIC void phf_destroy(struct phf *);

#ifdef __cplusplus
}
#endif


/*
 * C 1 1 / G N U  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if PHF_HAVE_GENERIC

#define phf_init(f, k, ...) _Generic((k), \
	uint32_t*: phf_init_uint32((f), (k), __VA_ARGS__), \
	uint64_t*: phf_init_uint64((f), (k), __VA_ARGS__), \
	phf_string_t*: phf_init_string((f), (k), __VA_ARGS__))

#define phf_hash(f, k) _Generic((k), \
	uint32_t: phf_hash_uint32((f), (k)), \
	uint64_t: phf_hash_uint64((f), (k)), \
	phf_string_t: phf_hash_string((f), (k)))

#elif PHF_HAVE_BUILTIN_TYPES_COMPATIBLE_P && PHF_HAVE_BUILTIN_CHOOSE_EXPR

#define phf_choose(cond, a, b) __builtin_choose_expr(cond, a, b)
#define phf_istype(E, T) __builtin_types_compatible_p(__typeof__(E), T)

#define phf_init(f, k, ...) \
	phf_choose(phf_istype(*(k), uint32_t), phf_init_uint32((f), (k), __VA_ARGS__), \
	phf_choose(phf_istype(*(k), uint64_t), phf_init_uint64((f), (k), __VA_ARGS__), \
	phf_choose(phf_istype(*(k), phf_string_t), phf_init_string((f), (k), __VA_ARGS__))), (void)0) \

#define phf_hash(f, k) \
	phf_choose(phf_istype((k), uint32_t), phf_hash_uint32((f), (k)), \
	phf_choose(phf_istype((k), uint64_t), phf_hash_uint64((f), (k)), \
	phf_choose(phf_istype((k), phf_string_t), phf_hash_string((f), (k)))), (void)0) \

#endif


#ifdef __clang__
#pragma clang diagnostic pop
#elif PHF_GNUC_PREREQ(4, 6)
#pragma GCC diagnostic pop
#endif

#endif /* PHF_H */
