#ifndef TNT_MEMCACHE_VAL_H_
#define TNT_MEMCACHE_VAL_H_

typedef struct {

	int flags;
	unsigned long long cas;

	char * key;

	char * value;
	int value_size;

} tnt_memcache_val_t;

typedef struct {

	int count;
	tnt_memcache_val_t * values;

} tnt_memcache_vals_t;

#define TNT_MEMCACHE_VAL_COUNT(VS) ((VS)->count)
#define TNT_MEMCACHE_VAL_GET(VS, IDX) (&(VS)->values[IDX])

void
tnt_memcache_val_init(tnt_memcache_vals_t * values);

void
tnt_memcache_val_free(tnt_memcache_vals_t * values);

int
tnt_memcache_val_alloc(tnt_memcache_vals_t * values, int count);

#endif
