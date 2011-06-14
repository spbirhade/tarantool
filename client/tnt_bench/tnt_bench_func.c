
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

void
tnt_bench_func_init(tnt_bench_funcs_t * funcs)
{
	tnt_bench_list_init(&funcs->list, 1);
}

void
tnt_bench_func_free(tnt_bench_funcs_t * funcs)
{
	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&funcs->list, i) {
		tnt_bench_func_t * f =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_func_t*);
		free(f->name);
	}
	tnt_bench_list_free(&funcs->list);
}

tnt_bench_func_t*
tnt_bench_func_add(tnt_bench_funcs_t * funcs, char * name,
	tnt_benchf_t func)
{
	tnt_bench_func_t * f =
		tnt_bench_list_alloc(&funcs->list, sizeof(tnt_bench_func_t));
	if (f == NULL)
		return NULL;

	f->name = strdup(name);
	if (f->name == NULL) {
		tnt_bench_list_del_last(&funcs->list);
		return NULL;
	}

	f->func = func;
	return f;
}

tnt_bench_func_t*
tnt_bench_func_match(tnt_bench_funcs_t * funcs, char * name)
{
	tnt_bench_list_node_t * i;
	TNT_BENCH_LIST_FOREACH(&funcs->list, i) {
		tnt_bench_func_t * f =
			TNT_BENCH_LIST_VALUE(i, tnt_bench_func_t*);
		if (!strcmp(f->name, name))
			return f;
	}
	return NULL;
}
