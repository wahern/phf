/* ==========================================================================
 * phf.cc - Tiny perfect hash function library.
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
#include <limits.h>   /* CHAR_BIT SIZE_MAX */
#include <inttypes.h> /* PRIu32 PRIu64 PRIx64 */
#include <stdint.h>   /* UINT32_C UINT64_C uint32_t uint64_t */
#include <stdlib.h>   /* abort(3) calloc(3) free(3) qsort(3) */
#include <string.h>   /* memset(3) */
#include <errno.h>    /* errno */
#include <assert.h>   /* assert(3) */
#if !PHF_NO_LIBCXX
#include <string>     /* std::string */
#endif

#include "phf.h"


#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#if __cplusplus < 201103L
#pragma clang diagnostic ignored "-Wc++11-long-long"
#endif
#elif PHF_GNUC_PREREQ(4, 6)
#pragma GCC diagnostic ignored "-Wunused-function"
#if __cplusplus < 201103L
#pragma GCC diagnostic ignored "-Wlong-long"
#pragma GCC diagnostic ignored "-Wformat" // %zu
#endif
#endif


/*
 * M A C R O  R O U T I N E S
 *
 * Mostly copies of <sys/param.h>
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define PHF_BITS(T) (sizeof (T) * CHAR_BIT)
#define PHF_HOWMANY(x, y) (((x) + ((y) - 1)) / (y))
#define PHF_MIN(a, b) (((a) < (b))? (a) : (b))
#define PHF_MAX(a, b) (((a) > (b))? (a) : (b))
#define PHF_ROTL(x, y) (((x) << (y)) | ((x) >> (PHF_BITS(x) - (y))))


/*
 * M O D U L A R  A R I T H M E T I C  R O U T I N E S
 *
 * Two modular reduction schemes are supported: bitwise AND and naive
 * modular division. For bitwise AND we must round up the values r and m to
 * a power of 2.
 *
 * TODO: Implement and test Barrett reduction as alternative to naive
 * modular division.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* round up to nearest power of 2 */
static inline size_t phf_powerup(size_t i) {
#if defined SIZE_MAX
	i--;
	i |= i >> 1;
	i |= i >> 2;
	i |= i >> 4;
	i |= i >> 8;
	i |= i >> 16;
#if SIZE_MAX != 0xffffffffu
	i |= i >> 32;
#endif
	return ++i;
#else
#error No SIZE_MAX defined
#endif
} /* phf_powerup() */


/*
 * B I T M A P  R O U T I N E S
 *
 * We use a bitmap to track output hash occupancy when searching for
 * displacement values.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef unsigned long phf_bits_t;

static inline bool phf_isset(phf_bits_t *set, size_t i) {
	return set[i / PHF_BITS(*set)] & ((size_t)1 << (i % PHF_BITS(*set)));
} /* phf_isset() */

static inline void phf_setbit(phf_bits_t *set, size_t i) {
	set[i / PHF_BITS(*set)] |= ((size_t)1 << (i % PHF_BITS(*set)));
} /* phf_setbit() */

static inline void phf_clrbit(phf_bits_t *set, size_t i) {
	set[i / PHF_BITS(*set)] &= ~((size_t)1 << (i % PHF_BITS(*set)));
} /* phf_clrbit() */

static inline void phf_clrall(phf_bits_t *set, size_t n) {
	memset(set, '\0', PHF_HOWMANY(n, PHF_BITS(*set)) * sizeof *set);
} /* phf_clrall() */


/*
 * H A S H  P R I M I T I V E S
 *
 * Universal hash based on MurmurHash3_x86_32. Variants for 32- and 64-bit
 * integer keys, and string keys.
 *
 * We use a random seed to address the non-cryptographic-strength collision
 * resistance of MurmurHash3. A stronger hash like SipHash is just too slow
 * and unnecessary for my particular needs. For some environments a
 * cryptographically stronger hash may be prudent.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static inline uint32_t phf_round32(uint32_t k1, uint32_t h1) {
	k1 *= UINT32_C(0xcc9e2d51);
	k1 = PHF_ROTL(k1, 15);
	k1 *= UINT32_C(0x1b873593);

	h1 ^= k1;
	h1 = PHF_ROTL(h1, 13);
	h1 = h1 * 5 + UINT32_C(0xe6546b64);

	return h1;
} /* phf_round32() */

static inline uint32_t phf_round32(const unsigned char *p, size_t n, uint32_t h1) {
	uint32_t k1;

	while (n >= 4) {
		k1 = (p[0] << 24)
		   | (p[1] << 16)
		   | (p[2] << 8)
		   | (p[3] << 0);

		h1 = phf_round32(k1, h1);

		p += 4;
		n -= 4;
	}

	k1 = 0;

	switch (n & 3) {
	case 3:
		k1 |= p[2] << 8;
	case 2:
		k1 |= p[1] << 16;
	case 1:
		k1 |= p[0] << 24;
		h1 = phf_round32(k1, h1);
	}

	return h1;
} /* phf_round32() */

static inline uint32_t phf_round32(phf_string_t k, uint32_t h1) {
	return phf_round32(reinterpret_cast<const unsigned char *>(k.p), k.n, h1);
} /* phf_round32() */

#if !PHF_NO_LIBCXX
static inline uint32_t phf_round32(std::string k, uint32_t h1) {
	return phf_round32(reinterpret_cast<const unsigned char *>(k.c_str()), k.length(), h1);
} /* phf_round32() */
#endif

static inline uint32_t phf_mix32(uint32_t h1) {
	h1 ^= h1 >> 16;
	h1 *= UINT32_C(0x85ebca6b);
	h1 ^= h1 >> 13;
	h1 *= UINT32_C(0xc2b2ae35);
	h1 ^= h1 >> 16;

	return h1;
} /* phf_mix32() */


/*
 * g(k) & f(d, k)  S P E C I A L I Z A T I O N S
 *
 * For every key we first calculate g(k). Then for every group of collisions
 * from g(k) we search for a displacement value d such that f(d, k) places
 * each key into a unique hash slot.
 *
 * g() and f() are specialized for 32-bit, 64-bit, and string keys.
 *
 * g_mod_r() and f_mod_n() are specialized for the method of modular
 * reduction--modular division or bitwise AND. bitwise AND is substantially
 * faster than modular division, and more than makes up for any space
 * inefficiency, particularly for small hash tables.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* 32-bit, phf_string_t, and std::string keys */
template<typename T>
static inline uint32_t phf_g(T k, uint32_t seed) {
	uint32_t h1 = seed;

	h1 = phf_round32(k, h1);

	return phf_mix32(h1);
} /* phf_g() */

template<typename T>
static inline uint32_t phf_f(uint32_t d, T k, uint32_t seed) {
	uint32_t h1 = seed;

	h1 = phf_round32(d, h1);
	h1 = phf_round32(k, h1);

	return phf_mix32(h1);
} /* phf_f() */


/* 64-bit keys */
static inline uint32_t phf_g(uint64_t k, uint32_t seed) {
	uint32_t h1 = seed;

	h1 = phf_round32(k, h1);
	h1 = phf_round32(k >> 32, h1);

	return phf_mix32(h1);
} /* phf_g() */

static inline uint32_t phf_f(uint32_t d, uint64_t k, uint32_t seed) {
	uint32_t h1 = seed;

	h1 = phf_round32(d, h1);
	h1 = phf_round32(static_cast<uint32_t>(k), h1);
	h1 = phf_round32(static_cast<uint32_t>(k >> 32), h1);

	return phf_mix32(h1);
} /* phf_f() */


/* g() and f() which parameterize modular reduction */
template<bool nodiv, typename T>
static inline uint32_t phf_g_mod_r(T k, uint32_t seed, size_t r) {
	return (nodiv)? (phf_g(k, seed) & (r - 1)) : (phf_g(k, seed) % r);
} /* phf_g_mod_r() */

template<bool nodiv, typename T>
static inline uint32_t phf_f_mod_m(uint32_t d, T k, uint32_t seed, size_t m) {
	return (nodiv)? (phf_f(d, k, seed) & (m - 1)) : (phf_f(d, k, seed) % m);
} /* phf_f_mod_m() */


/*
 * B U C K E T  S O R T I N G  I N T E R F A C E S
 *
 * For every key [0..n) we calculate g(k) % r, where 0 < r <= n, and
 * associate it with a bucket [0..r). We then sort the buckets in decreasing
 * order according to the number of keys. The sorting is required for both
 * optimal time complexity when calculating f(d, k) (less contention) and
 * optimal space complexity (smaller d).
 *
 * The actual sorting is done in the core routine. The buckets are organized
 * and sorted as a 1-dimensional array to minimize run-time memory (less
 * data structure overhead) and improve data locality (less pointer
 * indirection). The following section merely implements a templated
 * bucket-key structure and the comparison routine passed to qsort(3).
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool operator==(const phf_string_t &a, const phf_string_t &b) {
	return a.n == b.n && 0 == memcmp(a.p, b.p, a.n);
}

template<typename T>
struct phf_key {
	T k;
	phf_hash_t g; /* result of g(k) % r */
	size_t *n;  /* number of keys in bucket g */
}; /* struct phf_key */

template<typename T>
static int phf_keycmp(const phf_key<T> *a, const phf_key<T> *b) {
	if (*(a->n) > *(b->n))
		return -1;
	if (*(a->n) < *(b->n))
		return 1;
	if (a->g > b->g)
		return -1;
	if (a->g < b->g)
		return 1;

	/* duplicate key? */
	if (a->k == b->k && a != b) {
		assert(!(a->k == b->k));
		abort(); /* if NDEBUG defined */
	}

	return 0;
} /* phf_keycmp() */


/*
 * C O R E  F U N C T I O N  G E N E R A T O R
 *
 * The entire algorithm is contained in PHF:init. Everything else in this
 * source file is either a simple utility routine used by PHF:init, or an
 * interface to PHF:init or the generated function state.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

template<typename key_t, bool nodiv>
PHF_PUBLIC int PHF::init(struct phf *phf, const key_t k[], const size_t n, const size_t l, const size_t a, const phf_seed_t seed) {
	size_t n1 = PHF_MAX(n, 1); /* for computations that require n > 0 */
	size_t l1 = PHF_MAX(l, 1);
	size_t a1 = PHF_MAX(PHF_MIN(a, 100), 1);
	size_t r; /* number of buckets */
	size_t m; /* size of output array */
	phf_key<key_t> *B_k = NULL; /* linear bucket-slot array */
	size_t *B_z = NULL;         /* number of slots per bucket */
	phf_key<key_t> *B_p, *B_pe;
	phf_bits_t *T = NULL; /* bitmap to track index occupancy */
	phf_bits_t *T_b;      /* per-bucket working bitmap */
	size_t T_n;
	uint32_t *g = NULL; /* displacement map */
	uint32_t d_max = 0; /* maximum displacement value */
	int error;

	if ((phf->nodiv = nodiv)) {
		/* round to power-of-2 so we can use bit masks instead of modulo division */
		r = phf_powerup(n1 / PHF_MIN(l1, n1));
		m = phf_powerup((n1 * 100) / a1);
	} else {
		/* XXX: should we bother rounding to prime number? */
		r = PHF_HOWMANY(n1, l1);
		m = (n1 * 100) / a1;
	}

	if (!(B_k = static_cast<phf_key<key_t> *>(calloc(n1, sizeof *B_k))))
		goto syerr;
	if (!(B_z = static_cast<size_t *>(calloc(r, sizeof *B_z))))
		goto syerr;

	for (size_t i = 0; i < n; i++) {
		phf_hash_t g = phf_g_mod_r<nodiv>(k[i], seed, r);

		B_k[i].k = k[i];
		B_k[i].g = g;
		B_k[i].n = &B_z[g];
		++*B_k[i].n;
	}

	qsort(B_k, n1, sizeof *B_k, reinterpret_cast<int(*)(const void *, const void *)>(&phf_keycmp<key_t>));

	T_n = PHF_HOWMANY(m, PHF_BITS(*T));
	if (!(T = static_cast<phf_bits_t *>(calloc(T_n * 2, sizeof *T))))
		goto syerr;
	T_b = &T[T_n]; /* share single allocation */

	/*
	 * FIXME: T_b[] is unnecessary. We could clear T[] the same way we
	 * clear T_b[]. In fact, at the end of generation T_b[] is identical
	 * to T[] because we don't clear T_b[] on success.
	 *
	 * We just need to tweak the current reset logic to stop before the
	 * key that failed, and then we can elide the commit to T[] at the
	 * end of the outer loop.
	 */

	if (!(g = static_cast<uint32_t *>(calloc(r, sizeof *g))))
		goto syerr;

	B_p = B_k;
	B_pe = &B_k[n];

	for (; B_p < B_pe && *B_p->n > 0; B_p += *B_p->n) {
		phf_key<key_t> *Bi_p, *Bi_pe;
		size_t d = 0;
		uint32_t f;
retry:
		d++;
		Bi_p = B_p;
		Bi_pe = B_p + *B_p->n;

		for (; Bi_p < Bi_pe; Bi_p++) {
			f = phf_f_mod_m<nodiv>(d, Bi_p->k, seed, m);

			if (phf_isset(T, f) || phf_isset(T_b, f)) {
				/* reset T_b[] */
				for (Bi_p = B_p; Bi_p < Bi_pe; Bi_p++) {
					f = phf_f_mod_m<nodiv>(d, Bi_p->k, seed, m);
					phf_clrbit(T_b, f);
				}

				goto retry;
			} else {
				phf_setbit(T_b, f);
			}
		}

		/* commit to T[] */
		for (Bi_p = B_p; Bi_p < Bi_pe; Bi_p++) {
			f = phf_f_mod_m<nodiv>(d, Bi_p->k, seed, m);
			phf_setbit(T, f);
		}

		/* commit to g[] */
		g[B_p->g] = d;
		d_max = PHF_MAX(d, d_max);
	}

	phf->seed = seed;
	phf->r = r;
	phf->m = m;

	phf->g = g;
	g = NULL;

	phf->d_max = d_max;

	error = 0;

	goto clean;
syerr:
	error = errno;
clean:
	free(g);
	free(T);
	free(B_z);
	free(B_k);

	return error;
} /* PHF::init() */


/*
 * F U N C T I O N  G E N E R A T O R  &  S T A T E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

template int PHF::init<uint32_t, true>(struct phf *, const uint32_t[], const size_t, const size_t, const size_t, const phf_seed_t);
template int PHF::init<uint64_t, true>(struct phf *, const uint64_t[], const size_t, const size_t, const size_t, const phf_seed_t);
template int PHF::init<phf_string_t, true>(struct phf *, const phf_string_t[], const size_t, const size_t, const size_t, const phf_seed_t);
#if !PHF_NO_LIBCXX
template int PHF::init<std::string, true>(struct phf *, const std::string[], const size_t, const size_t, const size_t, const phf_seed_t);
#endif

template int PHF::init<uint32_t, false>(struct phf *, const uint32_t[], const size_t, const size_t, const size_t, const phf_seed_t);
template int PHF::init<uint64_t, false>(struct phf *, const uint64_t[], const size_t, const size_t, const size_t, const phf_seed_t);
template int PHF::init<phf_string_t, false>(struct phf *, const phf_string_t[], const size_t, const size_t, const size_t, const phf_seed_t);
#if !PHF_NO_LIBCXX
template int PHF::init<std::string, false>(struct phf *, const std::string[], const size_t, const size_t, const size_t, const phf_seed_t);
#endif

template<typename T>
PHF_PUBLIC phf_hash_t PHF::hash(struct phf *phf, T k) {
	if (phf->nodiv) {
		uint32_t d = phf->g[phf_g(k, phf->seed) & (phf->r - 1)];

		return phf_f(d, k, phf->seed) & (phf->m - 1);
	} else {
		uint32_t d = phf->g[phf_g(k, phf->seed) % phf->r];

		return phf_f(d, k, phf->seed) % phf->m;
	}
} /* PHF::hash() */

template phf_hash_t PHF::hash<uint32_t>(struct phf *, uint32_t);
template phf_hash_t PHF::hash<uint64_t>(struct phf *, uint64_t);
template phf_hash_t PHF::hash<phf_string_t>(struct phf *, phf_string_t);
#if !PHF_NO_LIBCXX
template phf_hash_t PHF::hash<std::string>(struct phf *, std::string);
#endif

PHF_PUBLIC void PHF::destroy(struct phf *phf) {
	free(phf->g);
	phf->g = NULL;
} /* PHF::destroy() */


PHF_PUBLIC int phf_init_uint32(struct phf *phf, uint32_t *k, size_t n, size_t lambda, size_t alpha, phf_seed_t seed, bool nodiv) {
	if (nodiv)
		return PHF::init<uint32_t, true>(phf, k, n, lambda, alpha, seed);
	else
		return PHF::init<uint32_t, false>(phf, k, n, lambda, alpha, seed);
} /* phf_init_uint32() */

PHF_PUBLIC int phf_init_uint64(struct phf *phf, uint64_t *k, size_t n, size_t lambda, size_t alpha, phf_seed_t seed, bool nodiv) {
	if (nodiv)
		return PHF::init<uint64_t, true>(phf, k, n, lambda, alpha, seed);
	else
		return PHF::init<uint64_t, false>(phf, k, n, lambda, alpha, seed);
} /* phf_init_uint64() */

PHF_PUBLIC int phf_init_string(struct phf *phf, phf_string_t *k, size_t n, size_t lambda, size_t alpha, phf_seed_t seed, bool nodiv) {
	if (nodiv)
		return PHF::init<phf_string_t, true>(phf, k, n, lambda, alpha, seed);
	else
		return PHF::init<phf_string_t, false>(phf, k, n, lambda, alpha, seed);
} /* phf_init_string() */

PHF_PUBLIC phf_hash_t phf_hash_uint32(struct phf *phf, uint32_t k) {
	return PHF::hash(phf, k);
} /* phf_hash_uint32() */

PHF_PUBLIC phf_hash_t phf_hash_uint64(struct phf *phf, uint64_t k) {
	return PHF::hash(phf, k);
} /* phf_hash_uint64() */

PHF_PUBLIC phf_hash_t phf_hash_string(struct phf *phf, phf_string_t k) {
	return PHF::hash(phf, k);
} /* phf_hash_string() */

PHF_PUBLIC void phf_destroy(struct phf *phf) {
	PHF::destroy(phf);
} /* phf_destroy() */


#if PHF_LUALIB
#include <time.h> /* time(2) */

#include <lua.hpp>


#if LUA_VERSION_NUM < 502
static int lua_absindex(lua_State *L, int idx) {
	return (idx > 0 || idx <= LUA_REGISTRYINDEX)? idx : lua_gettop(L) + idx + 1;
} /* lua_absindex() */

#define lua_rawlen(t, index) lua_objlen(t, index)
#endif


struct phfctx {
	int (*hash)(struct phf *, lua_State *, int index);
	struct phf ctx;
}; /* struct phfctx */


static int phf_hash_uint32(struct phf *phf, lua_State *L, int index) {
	uint32_t k = static_cast<uint32_t>(luaL_checkinteger(L, index));

	lua_pushinteger(L, static_cast<lua_Integer>(PHF::hash(phf, k) + 1));

	return 1;
} /* phf_hash_uint32() */

static int phf_hash_uint64(struct phf *phf, lua_State *L, int index) {
	uint64_t k = static_cast<uint64_t>(luaL_checkinteger(L, index));

	lua_pushinteger(L, static_cast<lua_Integer>(PHF::hash(phf, k) + 1));

	return 1;
} /* phf_hash_uint64() */

static int phf_hash_string(struct phf *phf, lua_State *L, int index) {
	phf_string_t k;

	k.p = const_cast<char *>(luaL_checklstring(L, index, &k.n));

	lua_pushinteger(L, static_cast<lua_Integer>(PHF::hash(phf, k) + 1));

	return 1;
} /* phf_hash_string() */

static phf_seed_t phf_seed(lua_State *L) {
	return phf_g(static_cast<uint32_t>(reinterpret_cast<intptr_t>(L)), static_cast<uint32_t>(time(NULL)));
} /* phf_seed() */

static int phf_mergekeys(lua_State *L, int from, int to) {
	size_t i, n;

	from = lua_absindex(L, from);
	to = lua_absindex(L, to);

	n = lua_rawlen(L, from);
	n = PHF_MIN(INT_MAX - 1, n);

	for (i = 1; i <= n; i++) {
		lua_rawgeti(L, from, i);

		if (lua_type(L, -1) == LUA_TNUMBER) {
			lua_Integer k = lua_tointeger(L, -1);

			lua_pop(L, 1);

			lua_pushinteger(L, k);
			lua_pushboolean(L, 1);
			lua_settable(L, to);
		} else {
			lua_pop(L, 1);

			break;
		}
	}

	if (i <= n) {
		for (i = 1; i <= n; i++) {
			lua_rawgeti(L, from, i);
			lua_tostring(L, -1);
			lua_pushboolean(L, 1);
			lua_settable(L, to);
		}

		return LUA_TSTRING;
	} else {
		return LUA_TNUMBER;
	}
} /* phf_mergekeys() */

template<typename T>
static phf_error_t phf_reallocarray(T **p, size_t count) {
	T *tmp;

	if (SIZE_MAX / sizeof **p < count)
		return ENOMEM;

	if (!(tmp = static_cast<T*>(realloc(*p, count * sizeof **p))))
		return errno;

	*p = tmp;

	return 0;
} /* phf_reallocarray() */

static bool phf_tokey(lua_State *L, int index, phf_string_t *k) {
	/* integer keys may exist in our merged table */
	if (lua_type(L, index) == LUA_TSTRING) {
		k->p = const_cast<char *>(lua_tolstring(L, index, &k->n));

		return 1;
	} else {
		return 0;
	}
} /* phf_tokey() */

static bool phf_tokey(lua_State *L, int index, uint64_t *k) {
	/* we should never encounter a string in our merged table */
	*k = static_cast<uint64_t>(lua_tointeger(L, index));

	return 1;
} /* phf_tokey() */

template<typename T>
static int phf_addkey(lua_State *L, int index, T **keys, size_t *n, size_t *z) {
	T k;
	int error;

	if (phf_tokey(L, index, &k)) {
		if (!(*n < *z)) {
			size_t count = PHF_MAX(*z, 512) * 2;

			if ((error = phf_reallocarray(keys, count)))
				return error;

			*z = count;
		}

		(*keys)[(*n)++] = k;
	}

	return 0;
} /* phf_addkey() */

template<typename T>
static phf_error_t phf_addkeys(lua_State *L, int index, T **keys, size_t *n, size_t *z) {
	int error;

	index = lua_absindex(L, index);

	lua_pushnil(L);

	while (lua_next(L, index)) {
		if ((error = phf_addkey<T>(L, -2, keys, n, z))) {
			lua_pop(L, 2);

			return error;
		}

		lua_pop(L, 1);
	}

	return 0;
} /* phf_addkeys() */

static int phf_new(lua_State *L) {
	size_t l = static_cast<size_t>(luaL_optinteger(L, 2, 4));
	size_t a = static_cast<size_t>(luaL_optinteger(L, 3, 80));
	phf_seed_t seed = (lua_isnoneornil(L, 4))? phf_seed(L) : static_cast<phf_seed_t>(luaL_checkinteger(L, 4));
	bool nodiv = static_cast<bool>(lua_toboolean(L, 5));
	void *keys = NULL;
	size_t n = 0, z = 0;
	struct phfctx *phf;
	int type, error;

	lua_settop(L, 5);
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_newtable(L); /* merged key table */

	type = phf_mergekeys(L, 1, 6);

	phf = static_cast<struct phfctx *>(lua_newuserdata(L, sizeof *phf));
	memset(phf, 0, sizeof *phf);

	luaL_getmetatable(L, "PHF*");
	lua_setmetatable(L, -2);

	if (type == LUA_TNUMBER) {
		if ((error = phf_addkeys(L, 6, reinterpret_cast<uint64_t **>(&keys), &n, &z)))
			goto error;

		if (n == 0)
			goto empty;

		if ((error = phf_init_uint64(&phf->ctx, reinterpret_cast<uint64_t *>(keys), n, l, a, seed, nodiv)))
			goto error;

		phf->hash = &phf_hash_uint64;
	} else {
		if ((error = phf_addkeys(L, 6, reinterpret_cast<phf_string_t **>(&keys), &n, &z)))
			goto error;

		if (n == 0)
			goto empty;

		if ((error = phf_init_string(&phf->ctx, reinterpret_cast<phf_string_t *>(keys), n, l, a, seed, nodiv)))
			goto error;

		phf->hash = &phf_hash_string;
	}

	free(keys);

	return 1;
empty:
	free(keys);

	lua_pushstring(L, "empty key set");

	return lua_error(L);
error:
	free(keys);

	lua_pushstring(L, strerror(error));

	return lua_error(L);
} /* phf_new() */

static int phf_r(lua_State *L) {
	struct phfctx *phf = static_cast<struct phfctx *>(luaL_checkudata(L, 1, "PHF*"));

	lua_pushinteger(L, static_cast<lua_Integer>(phf->ctx.r));

	return 1;
} /* phf_r() */

static int phf_m(lua_State *L) {
	struct phfctx *phf = static_cast<struct phfctx *>(luaL_checkudata(L, 1, "PHF*"));

	lua_pushinteger(L, static_cast<lua_Integer>(phf->ctx.m));

	return 1;
} /* phf_m() */

static int (phf_hash)(lua_State *L) {
	struct phfctx *phf = static_cast<struct phfctx *>(luaL_checkudata(L, 1, "PHF*"));

	return phf->hash(&phf->ctx, L, 2);
} /* phf_hash() */

static int phf__gc(lua_State *L) {
	struct phfctx *phf = (struct phfctx *)luaL_checkudata(L, 1, "PHF*");

	phf_destroy(&phf->ctx);

	return 0;
} /* phf__gc() */

static const luaL_Reg phf_methods[] = {
	{ "hash", &(phf_hash) },
	{ "r",    &phf_r },
	{ "m",    &phf_m },
	{ NULL,   NULL },
}; /* phf_methods[] */

static const luaL_Reg phf_metatable[] = {
	{ "__call", &phf_hash },
	{ "__gc",   &phf__gc },
	{ NULL,     NULL },
}; /* phf_metatable[] */

static const luaL_Reg phf_globals[] = {
	{ "new", &phf_new },
	{ NULL,  NULL },
}; /* phf_globals[] */

static void phf_register(lua_State *L, const luaL_Reg *l) {
#if LUA_VERSION_NUM >= 502
	luaL_setfuncs(L, l, 0);
#else
	luaL_register(L, NULL, l);
#endif
} /* phf_register() */

extern "C" int luaopen_phf(lua_State *L) {
	if (luaL_newmetatable(L, "PHF*")) {
		phf_register(L, phf_metatable);
		lua_newtable(L);
		phf_register(L, phf_methods);
		lua_setfield(L, -2, "__index");
	}

	lua_pop(L, 1);

	lua_newtable(L);
	phf_register(L, phf_globals);

	return 1;
} /* luaopen_phf() */

#endif /* PHF_LUALIB */


#if PHF_MAIN

#include <stdlib.h>    /* arc4random(3) free(3) realloc(3) */
#include <stdio.h>     /* fclose(3) fopen(3) fprintf(3) fread(3) freopen(3) printf(3) */
#include <time.h>      /* CLOCKS_PER_SEC clock(3) */
#include <string.h>    /* strcmp(3) */
#include <sys/param.h> /* BSD */
#include <unistd.h>    /* getopt(3) */
#include <strings.h>   /* ffsl(3) */
#include <err.h>       /* err(3) errx(3) warnx(3) */


static uint32_t randomseed(void) {
#if defined BSD /* catchall for modern BSDs, which all have arc4random */
	return arc4random();
#else
	FILE *fp;
	uint32_t seed;

	if (!(fp = fopen("/dev/urandom", "r")))
		err(1, "/dev/urandom");

	if (1 != fread(&seed, sizeof seed, 1, fp))
		err(1, "/dev/urandom");

	fclose(fp);

	return seed;
#endif
} /* randomseed() */


template<typename T>
static void pushkey(T **k, size_t *n, size_t *z, T kn) {
	if (!(*n < *z)) {
		size_t z1 = PHF_MAX(*z, 1) * 2;
		T *p;

		if (z1 < *z || (SIZE_MAX / sizeof **k) < z1)
			errx(1, "addkey: %s", strerror(ERANGE));

		if (!(p = (T *)realloc(*k, z1 * sizeof **k)))
			err(1, "realloc");

		*k = p;
		*z = z1;
	}

	(*k)[(*n)++] = kn;
} /* pushkey() */


template<typename T>
static void addkey(T **k, size_t *n, size_t *z, const char *src) {
	pushkey(k, n, z, static_cast<T>(strtoull(src, NULL, 0)));
} /* addkey() */

static void addkey(phf_string_t **k, size_t *n, size_t *z, char *src, size_t len) {
	phf_string_t kn = { (void *)src, len };
	pushkey(k, n, z, kn);
} /* addkey() */

static void addkey(phf_string_t **k, size_t *n, size_t *z, char *src) {
	addkey(k, n, z, src, strlen(src));
} /* addkey() */


template<typename T>
static void addkeys(T **k, size_t *n, size_t *z, char **src, int count) {
	for (int i = 0; i < count; i++)
		addkey(k, n, z, src[i]);
} /* addkey() */

template<typename T>
static void addkeys(T **k, size_t *n, size_t *z, FILE *fp, char **data) {
	char *ln = NULL;
	size_t lz = 0;
	ssize_t len;

	(void)data;

	while ((len = getline(&ln, &lz, fp)) > 0) {
		if (--len > 0) {
			if (ln[len] == '\n')
				ln[len] = '\0';
			addkey(k, n, z, ln);
		}
	}
	
	free(ln);
} /* addkeys() */

/* slurp file into a single string and take pointers */
static void addkeys(phf_string_t **k, size_t *n, size_t *z, FILE *fp, char **data) {
	size_t p = 0, pe = 0, tp;
	char buf[BUFSIZ], *tmp;
	size_t buflen;

	while ((buflen = fread(buf, 1, sizeof buf, fp))) {
		if (buflen > (pe - p)) {
			if (~buflen < pe || 0 == (pe = phf_powerup(buflen + pe)))
				errx(1, "realloc: %s", strerror(ERANGE));
			if (!(tmp = (char *)realloc(*data, pe)))
				err(1, "realloc");
			*data = tmp;
		}

		memcpy(*data + p, buf, buflen);
		p += buflen;
	}

	for (p = 0; p < pe; ) {
		while (p < pe && (*data)[p] == '\n')
			p++;

		tp = p;

		while (p < pe && (*data)[p] != '\n')
			p++;
				
		if (p > tp)
			addkey(k, n, z, &(*data)[tp], (size_t)(p - tp));
	}
} /* addkeys() */


static inline void printkey(phf_string_t &k, phf_hash_t hash) {
	printf("%-32.*s : %" PHF_PRIuHASH "\n", (int)k.n, (char *)k.p, hash);
} /* printkey() */

#if !PHF_NO_LIBCXX
static inline void printkey(std::string &k, phf_hash_t hash) {
	printf("%-32s : %" PHF_PRIuHASH "\n", k.c_str(), hash);
} /* printkey() */
#endif

template<typename T>
static inline void printkey(T k, phf_hash_t hash) {
	printf("%llu : %" PHF_PRIuHASH "\n", (unsigned long long)k, hash);
} /* printkey() */

template<typename T, bool nodiv>
static inline void exec(int argc, char **argv, size_t lambda, size_t alpha, size_t seed, bool verbose, bool noprint) {
	T *k = NULL;
	size_t n = 0, z = 0;
	char *data = NULL;
	struct phf phf;
	clock_t begin, end;

	addkeys(&k, &n, &z, argv, argc);
	addkeys(&k, &n, &z, stdin, &data);

	if (verbose)
		warnx("loaded %zu keys", n);

	begin = clock();
	PHF::init<T, nodiv>(&phf, k, n, lambda, alpha, seed);
	end = clock();

	if (verbose) {
		warnx("found perfect hash for %zu keys in %fs", n, (double)(end - begin) / CLOCKS_PER_SEC);

		int d_bits = ffsl((long)phf_powerup(phf.d_max));
		double k_bits = ((double)phf.r * d_bits) / n;
		double g_load = (double)n / phf.r;
		warnx("r:%zu m:%zu d_max:%zu d_bits:%d k_bits:%.2f g_load:%.2f", phf.r, phf.m, phf.d_max, d_bits, k_bits, g_load);

		size_t x = 0;

		begin = clock();
		for (size_t i = 0; i < n; i++) {
			x += PHF::hash(&phf, k[i]);
		}
		end = clock();

		warnx("hashed %zu keys in %fs (x:%zu)", n, (double)(end - begin) / CLOCKS_PER_SEC, x);
	}

	if (!noprint) {
		for (size_t i = 0; i < n; i++) {
			printkey(k[i], PHF::hash(&phf, k[i]));
		}
	}

	phf_destroy(&phf);
	free(data);
	free(k);
} /* exec() */


int main(int argc, char **argv) {
	const char *path = "/dev/null";
	size_t lambda = 4;
	size_t alpha = 80;
	uint32_t seed = randomseed();
	bool verbose = 0;
	bool noprint = 0;
	bool nodiv = 0;
	enum {
		PHF_UINT32,
		PHF_UINT64,
		PHF_STRING
	} type = PHF_UINT32;
	extern char *optarg;
	extern int optind;
	int optc;

	while (-1 != (optc = getopt(argc, argv, "f:l:a:s:2t:nvh"))) {
		switch (optc) {
		case 'f':
			path = optarg;
			break;
		case 'l':
			lambda = strtoul(optarg, NULL, 0);
			break;
		case 'a':
			alpha = strtoul(optarg, NULL, 0);
			break;
		case 's':
			seed = strtoul(optarg, NULL, 0);
			break;
		case '2':
			nodiv = 1;
			break;
		case 't':
			if (!strcmp(optarg, "uint32")) {
				type = PHF_UINT32;
			} else if (!strcmp(optarg, "uint64")) {
				type = PHF_UINT64;
			} else if (!strcmp(optarg, "string")) {
				type = PHF_STRING;
			} else {
				errx(1, "%s: invalid key type", optarg);
			}

			break;
		case 'n':
			noprint = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			/* FALL THROUGH */
		default:
			fprintf(optc == 'h'? stdout : stderr,
				"%s [-f:l:a:s:t:2nvh] [key [...]]\n"
				"  -f PATH  read keys from PATH (- for stdin)\n"
				"  -l NUM   number of keys per displacement map bucket (reported as g_load)\n"
				"  -a PCT   hash table load factor (1%% - 100%%)\n"
				"  -s SEED  random seed\n"
				"  -t TYPE  parse and hash keys as uint32, uint64, or string\n"
				"  -2       avoid modular division by rounding r and m to power of 2\n"
				"  -n       do not print key-hash pairs\n"
				"  -v       report hashing status\n"
				"  -h       print usage message\n"
				"\n"
				"Report bugs to <william@25thandClement.com>\n",
				argv[0]
			);

			return optc == 'h'? 0 : 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (strcmp(path, "-") && !freopen(path, "r", stdin))
		err(1, "%s", path);

	switch (type) {
	case PHF_UINT32:
		if (nodiv)
			exec<uint32_t, true>(argc, argv, lambda, alpha, seed, verbose, noprint);
		else
			exec<uint32_t, false>(argc, argv, lambda, alpha, seed, verbose, noprint);
		break;
	case PHF_UINT64:
		if (nodiv)
			exec<uint64_t, true>(argc, argv, lambda, alpha, seed, verbose, noprint);
		else
			exec<uint64_t, false>(argc, argv, lambda, alpha, seed, verbose, noprint);
		break;
	case PHF_STRING:
		if (nodiv)
			exec<phf_string_t, true>(argc, argv, lambda, alpha, seed, verbose, noprint);
		else
			exec<phf_string_t, false>(argc, argv, lambda, alpha, seed, verbose, noprint);
		break;
	}

	return 0;
} /* main() */

#endif /* PHF_MAIN */
