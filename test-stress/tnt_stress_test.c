
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include <libtnt.h>
#include <test-stress/tnt_stress.h>

static long long
stress_time(void)
{
    long long tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    tm = ((long)tv.tv_sec)*1000;
    tm += tv.tv_usec/1000;

    return tm;
}

static void
stress_end(long long start, int count, stress_stat_t * stat)
{
	stat->tm  = stress_time() - start;
	stat->rps = (float)count / ((float)stat->tm / 1000);
}

void
stress_ping(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	tnt_result_t result;

	int key;
	long long start = stress_time();

	(void)flags;
	(void)bsize;

	for (key = 0 ; key < count ; key++) {

		result = tnt_ping(t, key);
	   
		if (result != TNT_EOK)
			printf("ping failed: %s\n", tnt_error(result));
	}

	tnt_flush(t);

	tnt_recv_t rcv; 

	for (key = 0 ; key < count ; key++) {

		tnt_recv_init(&rcv);

		result = tnt_recv(t, &rcv);

		if (result != TNT_EOK)
			printf("recv failed: %s\n", tnt_error(result));

		tnt_recv_free(&rcv);
	}

	stress_end(start, count, stat);
}

void
stress_insert(tnt_t * t, 
	int bsize, int count, int flags, stress_stat_t * stat)
{
	char * buf = malloc(bsize);

	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_result_t result;
	tnt_tuple_t tu;

	long long start = stress_time();
	int key;

	for (key = 0 ; key < count ; key++) {

		tnt_tuple_init(&tu);
		tnt_tuple_add(&tu, (char*)&key, sizeof(key));

		tnt_tuple_add(&tu, buf, bsize);

		result = tnt_insert(t, key, 0, flags, &tu);
	   
		if (result != TNT_EOK )
			printf("insert failed: %s\n", tnt_error(result));

		tnt_tuple_free(&tu);
	}

	tnt_flush(t);

	tnt_recv_t rcv;

	for (key = 0 ; key < count ; key++) {

		tnt_recv_init(&rcv);

		result = tnt_recv(t, &rcv);

		if (result != TNT_EOK)
			printf("recv failed: %s\n", tnt_error(result));

		tnt_recv_free(&rcv);
	}

	stress_end(start, count, stat);
	free(buf);
}

void
stress_update(tnt_t * t, 
	int bsize, int count, int flags, stress_stat_t * stat)
{
	char * buf = malloc(bsize);

	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_result_t result;
	tnt_update_t u;

	int key;
	long long start = stress_time();

	for (key = 0 ; key < count ; key++) {

		tnt_update_init(&u);
		tnt_update_add(&u, TNT_UPDATE_ASSIGN, 1, buf, bsize);

		result = tnt_update(t, key, 0, flags, (char*)&key, sizeof(key), &u);

		if (result != TNT_EOK )
			printf("update failed: %s\n", tnt_error(result));

		tnt_update_free(&u);
	}

	tnt_flush(t);

	tnt_recv_t rcv;

	for (key = 0 ; key < count ; key++) {

		tnt_recv_init(&rcv);

		result = tnt_recv(t, &rcv);

		if (result != TNT_EOK)
			printf("recv failed: %s\n", tnt_error(result));

		tnt_recv_free(&rcv);
	}

	stress_end(start, count, stat);
	free(buf);
}

void
stress_select(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	tnt_result_t result;
	tnt_tuples_t tuples;

	int key;
	long long start = stress_time();

	(void)bsize;
	(void)flags;

	for (key = 0 ; key < count  ; key++) {

		tnt_tuples_init(&tuples);
		tnt_tuple_t * tu = tnt_tuples_add(&tuples);

		tnt_tuple_add(tu, (char*)&key, sizeof(key));

		result = tnt_select(t, key, 0, 0, 0, 100, &tuples);

		if (result != TNT_EOK )
			printf("select failed: %s\n", tnt_error(result));

		tnt_tuples_free(&tuples);
	}

	tnt_flush(t);

	tnt_recv_t rcv;

	for (key = 0 ; key < count ; key++) {

		tnt_recv_init(&rcv);

		result = tnt_recv(t, &rcv);

		if (result != TNT_EOK)
			printf("recv failed: %s\n", tnt_error(result));

		/*
		tnt_tuple_t * t;
		tnt_tuple_field_t * f;

		TNT_RECV_FOREACH(&rcv, t) {
			TNT_TUPLE_FOREACH(t, f) {
				printf("  %lu ", f->size);
			}
			printf("\n");
		}
		*/

		tnt_recv_free(&rcv);
	}

	stress_end(start, count, stat);
}

void
stress_select_set(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	tnt_result_t result;
	tnt_tuples_t tuples;

	int key, c = 0;
	long long start = stress_time();
	int per = 50;

	(void)bsize;
	(void)flags;

	tnt_tuples_init(&tuples);

	for (key = 0 ; key < count  ; key++) {
		
		if (key % per == 0 && (key != 0) ) {

			result = tnt_select(t, key, 0, 0, 0, 50, &tuples);

			if (result != TNT_EOK )
				printf("select failed: %s\n", tnt_error(result));

			tnt_tuples_free(&tuples);
			tnt_tuples_init(&tuples);
			c++;
		}

		tnt_tuple_t * tu = tnt_tuples_add(&tuples);
		tnt_tuple_add(tu, (char*)&key, sizeof(key));
	}

	tnt_flush(t);

	tnt_recv_t rcv;

	for (key = 0 ; key < c ; key++) {

		tnt_recv_init(&rcv);

		result = tnt_recv(t, &rcv);

		if (result != TNT_EOK)
			printf("recv failed: %s\n", tnt_error(result));

		/*
		tnt_tuple_t * t;
		tnt_tuple_field_t * f;

		printf("rcv count %lu: ", TNT_RECV_COUNT(&rcv));

		TNT_RECV_FOREACH(&rcv, t) {

			TNT_TUPLE_FOREACH(t, f) {
				printf("%lu-", f->size);
			}

			printf("; ");
		}

		printf("\n\n");
		*/

		tnt_recv_free(&rcv);
	}

	stress_end(start, c, stat);
}
