#ifndef TNT_BENCH_REDIS_H_
#define TNT_BENCH_REDIS_H_

int
tnt_bench_redis_set(tnt_t * t, char * key, char * data, int data_size);

int
tnt_bench_redis_set_recv(tnt_t * t);

int
tnt_bench_redis_get(tnt_t * t, char * key);

int
tnt_bench_redis_get_recv(tnt_t * t, char ** data, int * data_size);

#endif
