
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

void
tnt_bench_test_init(tnt_bench_tests_t * tests)
{
	tnt_bench_list_init(&tests->list, 1);
}

void
tnt_bench_test_free(tnt_bench_tests_t * tests)
{
	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&tests->list, i) {
		tnt_bench_test_t * t =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_test_t*);
		tnt_bench_list_free(&t->list);
	}
	tnt_bench_list_free(&tests->list);
}

tnt_bench_test_t*
tnt_bench_test_add(tnt_bench_tests_t * tests, tnt_bench_func_t * func)
{
	tnt_bench_test_t * test =
		tnt_bench_list_alloc(&tests->list, sizeof(tnt_bench_test_t));
	if (test == NULL)
		return NULL;

	test->func = func;
	tnt_bench_list_init(&test->list, 1);
	return test;
}

void
tnt_bench_test_buf_add(tnt_bench_test_t * test, int buf)
{
	tnt_bench_test_buf_t * b =
		tnt_bench_list_alloc(&test->list, sizeof(tnt_bench_test_buf_t));

	b->buf = buf;
	memset(&b->avg, 0, sizeof(b->avg));
}

char*
tnt_bench_test_buf_list(tnt_bench_test_t * test)
{
	int pos = 0;
	static char list[256];
	bool first = true;
	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&test->list, i) {
		tnt_bench_test_buf_t * b =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_test_buf_t*);
		if (first) {
			pos += snprintf(list + pos, sizeof(list) - pos, "%d", b->buf);
			first = false;
		} else
			pos += snprintf(list + pos, sizeof(list) - pos, ", %d", b->buf);
	}
	return list;
}

int
tnt_bench_test_buf_max(tnt_bench_test_t * test)
{
	int max;
	if (TNT_BENCH_LIST_COUNT(&test->list))
		max = TNT_BENCH_LIST_VALUE(test->list.head, tnt_bench_test_buf_t*)->buf;

	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&test->list, i) {
		tnt_bench_test_buf_t * b =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_test_buf_t*);
		if (b->buf > max)
			max = b->buf;
	}
	return max;
}

int
tnt_bench_test_buf_min(tnt_bench_test_t * test)
{
	int min;
	if (TNT_BENCH_LIST_COUNT(&test->list))
		min = TNT_BENCH_LIST_VALUE(test->list.head, tnt_bench_test_buf_t*)->buf;

	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&test->list, i) {
		tnt_bench_test_buf_t * b =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_test_buf_t*);
		if (b->buf < min)
			min = b->buf;
	}
	return min;
}
