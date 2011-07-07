#include "third_party/khash.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

typedef uint32_t u32;
typedef int32_t i32;
typedef void* ptr_t;


#define id(key) (key)
#define cnst(key) 42
#define eq(a, b) (a == b)

KHASH_INIT(id, u32, ptr_t, 1, id, eq, realloc);
KHASH_INIT(cnst, u32, ptr_t, 1, id, eq, realloc);

;;;
#define khkey_t u32
#define khval_t ptr_t
#define kh_is_map 1
#define __hash_func id
#define __hash_equal eq

typedef struct kh_cov {
	khint_t n_buckets, size, n_occupied, upper_bound;
	uint32_t *flags;
	khkey_t *keys;
	khval_t *vals;
} kh_cov_t;
static inline kh_cov_t *kh_init_cov() {
	return (kh_cov_t*)calloc(1, sizeof(kh_cov_t));
}
static inline void kh_destroy_cov(kh_cov_t *h)
{
	if (h) {
		free(h->keys);
		free(h->flags);
		free(h->vals);
		free(h);
	}
}
static inline void kh_clear_cov(kh_cov_t *h)
{
	if (h && h->flags) {
		memset(h->flags, 0xaa, ((h->n_buckets>>4) + 1) * sizeof(uint32_t));
		h->size = h->n_occupied = 0;
	}
}
static inline khint_t kh_get_cov(kh_cov_t *h, khkey_t key)
{
	if (h->n_buckets) {
		khint_t inc, k, i, last;
		k = __hash_func(key);
		last = i = k % h->n_buckets;
		inc = 1 + k % (h->n_buckets - 1);

		while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !__hash_equal(h->keys[i], key))) {
			if (i + inc >= h->n_buckets)
				i = i + inc - h->n_buckets;
			else
				i += inc;

			if (i == last)
				return h->n_buckets;
		}
		return __ac_iseither(h->flags, i)? h->n_buckets : i;
	} else return 0;
}
static inline void kh_resize_cov(kh_cov_t *h, khint_t new_n_buckets)
{
	uint32_t *new_flags = 0;
	khint_t j = 1;
	{
		khint_t t = __ac_HASH_PRIME_SIZE - 1;
		while (__ac_prime_list[t] > new_n_buckets) --t;
		new_n_buckets = __ac_prime_list[t+1];
		if (h->size >= (khint_t)(new_n_buckets * __ac_HASH_UPPER + 0.5))
			j = 0;
		else {
			new_flags = (uint32_t*)malloc(((new_n_buckets>>4) + 1) * sizeof(uint32_t));
			memset(new_flags, 0xaa, ((new_n_buckets>>4) + 1) * sizeof(uint32_t));
			if (h->n_buckets < new_n_buckets) {
				h->keys = (khkey_t*)realloc(h->keys, new_n_buckets * sizeof(khkey_t));
				if (kh_is_map)
					h->vals = (khval_t*)realloc(h->vals, new_n_buckets * sizeof(khval_t));
			}
		}
	}
	if (j) {
		for (j = 0; j != h->n_buckets; ++j) {
			if (__ac_iseither(h->flags, j) == 0) {
				khkey_t key = h->keys[j];
				khval_t val;
				if (kh_is_map)
					val = h->vals[j];
				__ac_set_isdel_true(h->flags, j);
				while (1) {
					khint_t inc, k, i;
					k = __hash_func(key);
					i = k % new_n_buckets;
					inc = 1 + k % (new_n_buckets - 1);
					while (!__ac_isempty(new_flags, i)) {
						if (i + inc >= new_n_buckets)
							i = i + inc - new_n_buckets;
						else
							i += inc;
					}
					__ac_set_isempty_false(new_flags, i);
					if (i < h->n_buckets && __ac_iseither(h->flags, i) == 0) {
						{ khkey_t tmp = h->keys[i]; h->keys[i] = key; key = tmp; }
						if (kh_is_map) {
							khval_t tmp = h->vals[i]; h->vals[i] = val; val = tmp;
						}
						__ac_set_isdel_true(h->flags, i);
					} else {
						h->keys[i] = key;
						if (kh_is_map)
							h->vals[i] = val;
						break;
					}
				}
			}
		}
		if (h->n_buckets > new_n_buckets) {
			h->keys = (khkey_t*)realloc(h->keys, new_n_buckets * sizeof(khkey_t));
			if (kh_is_map)
				h->vals = (khval_t*)realloc(h->vals, new_n_buckets * sizeof(khval_t));
		}
		free(h->flags);
		h->flags = new_flags;
		h->n_buckets = new_n_buckets;
		h->n_occupied = h->size;
		h->upper_bound = (khint_t)(h->n_buckets * __ac_HASH_UPPER + 0.5);
	}
}
static inline khint_t kh_put_cov(kh_cov_t *h, khkey_t key, int *ret)
{
	khint_t x;
	if (h->n_occupied >= h->upper_bound) {
		if (h->n_buckets > (h->size<<1))
			kh_resize_cov(h, h->n_buckets - 1);
		else
			kh_resize_cov(h, h->n_buckets + 1);
	}
	{
		khint_t inc, k, i, site, last;
		x = site = h->n_buckets; k = __hash_func(key); i = k % h->n_buckets;
		if (__ac_isempty(h->flags, i))
			x = i;
		else {
			inc = 1 + k % (h->n_buckets - 1); last = i;
			while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !__hash_equal(h->keys[i], key))) {
				if (__ac_isdel(h->flags, i))
					site = i;
				if (i + inc >= h->n_buckets)
					i = i + inc - h->n_buckets;
				else
					i += inc;
				if (i == last) {
					x = site;
					break;
				}
			}
			if (x == h->n_buckets) {
				if (__ac_isempty(h->flags, i) && site != h->n_buckets)
					x = site;
				else
					x = i;
			}
		}
	}
	if (__ac_isempty(h->flags, x)) {
		h->keys[x] = key;
		__ac_set_isboth_false(h->flags, x);
		++h->size; ++h->n_occupied;
		*ret = 1;
	} else if (__ac_isdel(h->flags, x)) {
		h->keys[x] = key;
		__ac_set_isboth_false(h->flags, x);
		++h->size;
		*ret = 2;
	} else *ret = 0;
	return x;
}
static inline void kh_del_cov(kh_cov_t *h, khint_t x)
{
	if (x != h->n_buckets && !__ac_iseither(h->flags, x)) {
		__ac_set_isdel_true(h->flags, x);
		--h->size;
	}
}

#undef khkey_t
#undef khval_t_t
#undef kh_is_map
#undef __hash_func
#undef __hash_equal

;;;
static void t1()
{
	int ret, k;
	khash_t(id) *h = kh_init(id, NULL);

#define init()		({ kh_init(id, NULL);		})
#define clear(x)	({ kh_clear(id, (x));		})
#define destroy(x)	({ kh_destroy(id, (x));		})
#define get(x)		({ kh_get(id, h, (x));		})
#define put(x)		({ kh_put(id, h, (x), &ret);	})
#define del(x)		({ kh_del(id, h, (x));		})

#include "body.x"
}

static void t2()
{
	int ret, k;
	khash_t(cnst) *h = kh_init(cnst, NULL);

#define init()		({ kh_init(cnst, NULL);		})
#define clear(x)	({ kh_clear(cnst, (x));		})
#define destroy(x)	({ kh_destroy(cnst, (x));	})
#define get(x)		({ kh_get(cnst, h, (x));	})
#define put(x)		({ kh_put(cnst, h, (x), &ret);	})
#define del(x)		({ kh_del(cnst, h, (x));	})

#include "body.x"
}


static void t3()
{
	int ret, k;
	khash_t(cov) *h;

#define init()		({ kh_init(cov, NULL);		})
#define clear(x)	({ kh_clear(cov, (x));		})
#define destroy(x)	({ kh_destroy(cov, (x));	})
#define get(x)		({ kh_get(cov, h, (x));		})
#define put(x)		({ kh_put(cov, h, (x), &ret);	})
#define del(x)		({ kh_del(cov, h, (x));		})

#include "body.x"
}

int main(void)
{
	t1();
	t2();
	t3();

	puts("ok");
	return 0;
}
