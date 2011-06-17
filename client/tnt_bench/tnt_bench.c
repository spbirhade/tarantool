
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

#include <client/tnt_bench/tnt_bench_list.h>
#include <client/tnt_bench/tnt_bench_stat.h>
#include <client/tnt_bench/tnt_bench_func.h>
#include <client/tnt_bench/tnt_bench_test.h>
#include <client/tnt_bench/tnt_bench_opt.h>
#include <client/tnt_bench/tnt_bench.h>
#include <client/tnt_bench/tnt_bench_cb.h>
#include <client/tnt_bench/tnt_bench_plot.h>

int
tnt_bench_init(tnt_bench_t * bench,
	tnt_bench_funcs_t * funcs, tnt_bench_opt_t * opt)
{
	bench->funcs = funcs;
	bench->opt = opt;

	tnt_bench_test_init(&bench->tests);

	bench->t = tnt_init(opt->proto, opt->rbuf, opt->sbuf);
	if (bench->t == NULL)
		return -1;

	if (tnt_set_auth(bench->t, opt->auth, opt->mech,
		opt->id,
		(unsigned char*)opt->key, opt->key_size) == -1)
		return -1;

	return 0;
}

void
tnt_bench_free(tnt_bench_t * bench)
{
	tnt_bench_test_free(&bench->tests);
	tnt_free(bench->t);
}

static void
tnt_bench_set_std(tnt_bench_t * bench)
{
	tnt_bench_func_t * f;
	tnt_bench_test_t * t;

	f = tnt_bench_func_match(bench->funcs, "insert");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);

	f = tnt_bench_func_match(bench->funcs, "insert-ret");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);

	f = tnt_bench_func_match(bench->funcs, "update");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);

	f = tnt_bench_func_match(bench->funcs, "update-ret");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);

	f = tnt_bench_func_match(bench->funcs, "select");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 0);
}

static void
tnt_bench_set_std_memcache(tnt_bench_t * bench)
{
	tnt_bench_func_t * f;
	tnt_bench_test_t * t;

	f = tnt_bench_func_match(bench->funcs, "memcache-set");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);

	f = tnt_bench_func_match(bench->funcs, "memcache-get");
	t = tnt_bench_test_add(&bench->tests, f);
	tnt_bench_test_buf_add(t, 32);
	tnt_bench_test_buf_add(t, 64);
	tnt_bench_test_buf_add(t, 128);
}

int
tnt_bench_connect(tnt_bench_t * bench)
{
	return tnt_connect(bench->t, bench->opt->host, bench->opt->port);
}

void
tnt_bench_run(tnt_bench_t * bench)
{
	/* using specified tests, if supplied */
	if (TNT_BENCH_LIST_COUNT(&bench->opt->tests)) {
		tnt_bench_list_node_t * iter;
		TNT_BENCH_LIST_FOREACH(&bench->opt->tests, iter) {
			char * name = TNT_BENCH_LIST_VALUE(iter, char*);
			tnt_bench_func_t * func = 
				tnt_bench_func_match(bench->funcs, name);
			if (func == NULL) {
				printf("unknown test: \"%s\", try --test-list\n", name);
				return;
			}
			tnt_bench_test_add(&bench->tests, func);
		}
		TNT_BENCH_LIST_FOREACH(&bench->opt->bufs, iter) {
			char * buf = TNT_BENCH_LIST_VALUE(iter, char*);
			tnt_bench_list_node_t * i;
			TNT_BENCH_LIST_FOREACH(&bench->tests.list, i) {
				tnt_bench_test_t * t =
					TNT_BENCH_LIST_VALUE(i, tnt_bench_test_t*);
				tnt_bench_test_buf_add(t, atoi(buf));
			}
		}
	} else {
		if (bench->opt->std) 
			tnt_bench_set_std(bench);
		else
		if (bench->opt->std_memcache) 
			tnt_bench_set_std_memcache(bench);
	}

	tnt_bench_stat_t * stats =
		malloc(sizeof(tnt_bench_stat_t) * bench->opt->reps);
	if (stats == NULL)
		return;

	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&bench->tests.list, i) {
		tnt_bench_test_t * t =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_test_t*);

		if (bench->opt->color)
			printf("\033[22;33m%s\033[0m\n", t->func->name);
		else
			printf("%s\n", t->func->name);
		fflush(stdout);

		tnt_bench_list_node_t * j;
		TNT_BENCH_LIST_FOREACH(&t->list, j) {
			tnt_bench_test_buf_t * b =
				TNT_BENCH_LIST_VALUE(j, tnt_bench_test_buf_t*);

			printf("  >>> [%d] ", b->buf);
			memset(stats, 0, sizeof(tnt_bench_stat_t) *
				bench->opt->reps);
			fflush(stdout);

			int r;
			for (r = 0 ; r < bench->opt->reps ; r++) {
				t->func->func(bench->t, b->buf, bench->opt->count, &stats[r]);
				printf("<%.2f %.2f> ", stats[r].rps, (float)stats[r].tm / 1000);
				fflush(stdout);
			}

			float rps = 0.0;
			unsigned long long tm = 0;
			for (r = 0 ; r < bench->opt->reps ; r++) {
				rps += stats[r].rps;
				tm += stats[r].tm;
			}

			b->avg.rps   = rps / bench->opt->reps;
			b->avg.tm    = (float)tm / 1000 / bench->opt->reps;
			b->avg.start = 0;
			b->avg.count = 0;

			printf("\n");

			if (bench->opt->color) 
				printf("  <<< (avg time \033[22;35m%.2f\033[0m sec): \033[22;32m%.2f\033[0m rps\n", 
					b->avg.tm, b->avg.rps);
			else
				printf("  <<< (avg time %.2f sec): %.2f rps\n", 
					b->avg.tm, b->avg.rps);
		}
	}

	free(stats);
	if (bench->opt->plot)
		tnt_bench_plot(bench);
}
