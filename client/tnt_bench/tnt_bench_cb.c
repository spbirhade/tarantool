
/*
 * Copyright (C) 2011 Mail.RU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libtnt.h>

#include <client/tnt_bench/tnt_bench_stat.h>
#include <client/tnt_bench/tnt_bench_list.h>
#include <client/tnt_bench/tnt_bench_func.h>
#include <client/tnt_bench/tnt_bench_cb.h>

static void
tnt_bench_cb_error(tnt_t * t, char * name)
{
	printf("%s failed: %s", name, tnt_perror(t));
	if (tnt_error(t) == TNT_ESYSTEM)
		printf("(%s)", strerror(tnt_error_errno(t)));
	printf("\n");
}

static void
tnt_bench_cb_recv(tnt_t * t, int count)
{
	int key;
	for (key = 0 ; key < count ; key++) {
		tnt_recv_t rcv; 
		tnt_recv_init(&rcv);
		if (tnt_recv(t, &rcv) == -1)
			tnt_bench_cb_error(t, "recv");
		else {
			if (tnt_error(t) != TNT_EOK)
				printf("server respond: %s (op: %d, reqid: %lu, code: %lu, count: %lu)\n",
					tnt_perror(t), TNT_RECV_OP(&rcv),
					TNT_RECV_ID(&rcv),
					TNT_RECV_CODE(&rcv),
					TNT_RECV_COUNT(&rcv));
		}
		tnt_recv_free(&rcv);
	}
}

static void
tnt_bench_cb_insert_do(tnt_t * t, char * name,
	int bsize, int count, int flags, tnt_bench_stat_t * stat)
{
	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", bsize, i);
		tnt_tuple_t tu;
		tnt_tuple_init(&tu);
		tnt_tuple_add(&tu, key, key_len);
		tnt_tuple_add(&tu, buf, bsize);
		if (tnt_insert(t, i, 0, flags, &tu) == -1)
			tnt_bench_cb_error(t, name);
		tnt_tuple_free(&tu);
	}

	tnt_flush(t);
	tnt_bench_cb_recv(t, count);
	tnt_bench_stat_stop(stat);

	free(buf);
}

static void
tnt_bench_cb_insert_do_sync(tnt_t * t, char * name,
	int bsize, int count, int flags, tnt_bench_stat_t * stat)
{
	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", bsize, i);
		tnt_tuple_t tu;
		tnt_tuple_init(&tu);
		tnt_tuple_add(&tu, key, key_len);
		tnt_tuple_add(&tu, buf, bsize);
		if (tnt_insert(t, i, 0, flags, &tu) == -1)
			tnt_bench_cb_error(t, name);
		tnt_flush(t);
		tnt_tuple_free(&tu);

		tnt_bench_cb_recv(t, 1);
	}

	tnt_bench_stat_stop(stat);
	free(buf);
}

static void
tnt_bench_cb_insert(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_insert_do(t, "insert",
		bsize, count, 0, stat);
}

static void
tnt_bench_cb_insert_ret(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_insert_do(t, "insert-ret",
		bsize, count, TNT_PROTO_FLAG_RETURN, stat);
}

static void
tnt_bench_cb_insert_sync(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_insert_do_sync(t, "sync-insert",
		bsize, count, 0, stat);
}

static void
tnt_bench_cb_insert_ret_sync(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_insert_do_sync(t, "sync-insert-ret",
		bsize, count, TNT_PROTO_FLAG_RETURN, stat);
}

static void
tnt_bench_cb_update_do(tnt_t * t, char * name,
	int bsize, int count, int flags, tnt_bench_stat_t * stat)
{
	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", bsize, i);
		tnt_update_t u;
		tnt_update_init(&u);
		tnt_update_add(&u, TNT_UPDATE_ASSIGN, 1, buf, bsize);
		if (tnt_update(t, i, 0, flags, key, key_len, &u) == -1)
			tnt_bench_cb_error(t, name);
		tnt_update_free(&u);
	}

	tnt_flush(t);
	tnt_bench_cb_recv(t, count);
	tnt_bench_stat_stop(stat);

	free(buf);
}

static void
tnt_bench_cb_update_do_sync(tnt_t * t, char * name,
	int bsize, int count, int flags, tnt_bench_stat_t * stat)
{
	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", bsize, i);
		tnt_update_t u;
		tnt_update_init(&u);
		tnt_update_add(&u, TNT_UPDATE_ASSIGN, 1, buf, bsize);
		if (tnt_update(t, i, 0, flags, key, key_len, &u) == -1)
			tnt_bench_cb_error(t, name);
		tnt_flush(t);
		tnt_update_free(&u);

		tnt_bench_cb_recv(t, 1);
	}

	tnt_bench_stat_stop(stat);
	free(buf);
}

static void
tnt_bench_cb_update(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_update_do(t, "update",
		bsize, count, 0, stat);
}

static void
tnt_bench_cb_update_ret(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_update_do(t, "update-ret",
		bsize, count, TNT_PROTO_FLAG_RETURN, stat);
}

static void
tnt_bench_cb_update_sync(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_update_do_sync(t, "sync-update",
		bsize, count, 0, stat);
}

static void
tnt_bench_cb_update_ret_sync(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	tnt_bench_cb_update_do_sync(t, "sync-update-ret",
		bsize, count, TNT_PROTO_FLAG_RETURN, stat);
}

static void
tnt_bench_cb_select(tnt_t * t, int bsize __attribute__((unused)),
	int count, tnt_bench_stat_t * stat)
{
	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", 0, i);
		tnt_tuples_t tuples;
		tnt_tuples_init(&tuples);
		tnt_tuple_t * tu = tnt_tuples_add(&tuples);
		tnt_tuple_add(tu, key, key_len);
		if (tnt_select(t, i, 0, 0, 0, 100, &tuples) == -1)
			tnt_bench_cb_error(t, "select");
		tnt_tuples_free(&tuples);
	}

	tnt_flush(t);
	tnt_bench_cb_recv(t, count);
	tnt_bench_stat_stop(stat);
}

static void
tnt_bench_cb_select_sync(tnt_t * t, int bsize __attribute__((unused)),
	int count, tnt_bench_stat_t * stat)
{
	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		int key_len = snprintf(key, sizeof(key), "key_%d_%d", 0, i);
		tnt_tuples_t tuples;
		tnt_tuples_init(&tuples);
		tnt_tuple_t * tu = tnt_tuples_add(&tuples);
		tnt_tuple_add(tu, key, key_len);
		if (tnt_select(t, i, 0, 0, 0, 100, &tuples) == -1)
			tnt_bench_cb_error(t, "sync-select");
		tnt_flush(t);
		tnt_tuples_free(&tuples);

		tnt_bench_cb_recv(t, 1);
	}

	tnt_bench_stat_stop(stat);
}

static void
tnt_bench_cb_ping(tnt_t * t, int bsize __attribute__((unused)),
	int count, tnt_bench_stat_t * stat)
{
	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		snprintf(key, sizeof(key), "key_%d_%d", 0, i);
		if (tnt_ping(t, i) == -1)
			tnt_bench_cb_error(t, "ping");
	}

	tnt_flush(t);
	tnt_bench_cb_recv(t, count);
	tnt_bench_stat_stop(stat);
}

static void
tnt_bench_cb_ping_sync(tnt_t * t, int bsize __attribute__((unused)),
	int count, tnt_bench_stat_t * stat)
{
	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		snprintf(key, sizeof(key), "key_%d_%d", 0, i);
		if (tnt_ping(t, i) == -1)
			tnt_bench_cb_error(t, "sync-ping");
		tnt_flush(t);
		tnt_bench_cb_recv(t, 1);
	}

	tnt_bench_stat_stop(stat);
}

static void
tnt_bench_cb_memcache_set(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat)
{
	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}
	memset(buf, 'x', bsize);

	tnt_bench_stat_start(stat, count);

	int i;
	for (i = 0 ; i < count ; i++) {
		char key[32];
		snprintf(key, sizeof(key), "key_%d_%d", bsize, i);

		if (tnt_memcache_set(t, 0, 0, key, buf, bsize) == -1)
			tnt_bench_cb_error(t, "set");
	}

	tnt_bench_stat_stop(stat);
	free(buf);
}

static void
tnt_bench_cb_memcache_get(tnt_t * t, int bsize __attribute__((unused)),
	int count, tnt_bench_stat_t * stat)
{
	tnt_bench_stat_start(stat, count);

	int key;
	char keydesc[32];
	char * keyptr[1] = { keydesc };

	for (key = 0 ; key < count ; key++) {
		snprintf(keydesc, sizeof(keydesc), "key_%d_%d", bsize, key);

		tnt_memcache_vals_t vals;
		tnt_memcache_val_init(&vals);

		if (tnt_memcache_get(t, false, 1, keyptr, &vals) == -1)
			tnt_bench_cb_error(t, "get");

		tnt_memcache_val_free(&vals);
	}

	tnt_bench_stat_stop(stat);
}

void
tnt_bench_cb_init(tnt_bench_funcs_t * funcs)
{
	tnt_bench_func_add(funcs, "insert", tnt_bench_cb_insert);
	tnt_bench_func_add(funcs, "insert-ret", tnt_bench_cb_insert_ret);
	tnt_bench_func_add(funcs, "update", tnt_bench_cb_update);
	tnt_bench_func_add(funcs, "update-ret", tnt_bench_cb_update_ret);
	tnt_bench_func_add(funcs, "select", tnt_bench_cb_select);
	tnt_bench_func_add(funcs, "ping", tnt_bench_cb_ping);

	tnt_bench_func_add(funcs, "sync-insert", tnt_bench_cb_insert_sync);
	tnt_bench_func_add(funcs, "sync-insert-ret", tnt_bench_cb_insert_ret_sync);
	tnt_bench_func_add(funcs, "sync-update", tnt_bench_cb_update_sync);
	tnt_bench_func_add(funcs, "sync-update-ret", tnt_bench_cb_update_ret_sync);
	tnt_bench_func_add(funcs, "sync-select", tnt_bench_cb_select_sync);
	tnt_bench_func_add(funcs, "sync-ping", tnt_bench_cb_ping_sync);

	tnt_bench_func_add(funcs, "memcache-set", tnt_bench_cb_memcache_set);
	tnt_bench_func_add(funcs, "memcache-get", tnt_bench_cb_memcache_get);
}
