#ifndef TNT_BENCH_FUNC_H_
#define TNT_BENCH_FUNC_H_

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

typedef void (*tnt_benchf_t)(tnt_t * t,
	int bsize, int count, tnt_bench_stat_t * stat);

typedef struct _tnt_bench_func_t  tnt_bench_func_t;
typedef struct _tnt_bench_funcs_t tnt_bench_funcs_t;

struct _tnt_bench_func_t {
	char * name;
	tnt_benchf_t func;
};

struct _tnt_bench_funcs_t {
	tnt_bench_list_t list;
};

void
tnt_bench_func_init(tnt_bench_funcs_t * funcs);

void
tnt_bench_func_free(tnt_bench_funcs_t * funcs);

tnt_bench_func_t*
tnt_bench_func_add(tnt_bench_funcs_t * funcs, char * name,
	tnt_benchf_t func);

tnt_bench_func_t*
tnt_bench_func_match(tnt_bench_funcs_t * funcs, char * name);

#endif
