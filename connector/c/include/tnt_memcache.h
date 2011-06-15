#ifndef TNT_MEMCACHE_H_
#define TNT_MEMCACHE_H_

int
tnt_memcache_set(tnt_t * t,
	int flags, int expire, char * key, char * data, int size);

int
tnt_memcache_add(tnt_t * t, int flags,
	int expire, char * key, char * data, int size);

int
tnt_memcache_replace(tnt_t * t, int flags,
	int expire, char * key, char * data, int size);

int
tnt_memcache_append(tnt_t * t, int flags,
	int expire, char * key, char * data, int size);

int
tnt_memcache_prepend(tnt_t * t, int flags,
	int expire, char * key, char * data, int size);

int
tnt_memcache_cas(tnt_t * t, int flags, int expire,
	unsigned long long cas, char * key, char * data, int size);

int
tnt_memcache_get(tnt_t * t, bool cas,
	int count, char ** keys, tnt_memcache_vals_t * values);

int
tnt_memcache_delete(tnt_t * t, char * key, int time);

int
tnt_memcache_inc(tnt_t * t, char * key,
	unsigned long long inc, unsigned long long * value);

int
tnt_memcache_dec(tnt_t * t, char * key,
	unsigned long long inc, unsigned long long * value);

int
tnt_memcache_flush_all(tnt_t * t, int time);

#endif
