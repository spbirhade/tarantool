
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
#include <client/tnt_bench/tnt_bench_opt.h>

void
tnt_bench_opt_init(tnt_bench_opt_t * opt)
{
	opt->auth = TNT_AUTH_CHAP;
	opt->proto = TNT_PROTO_RW;

	opt->host = "localhost";
	opt->port = 15312;

	opt->id = "test";
	opt->key = "1234567812345678";
	opt->key_size = 16;
	opt->mech = "PLAIN";

	opt->color = 1;
	opt->count = 1000;
	opt->reps = 3;

	opt->rbuf = 16384;
	opt->sbuf = 16384;

	opt->plot = 0;
	opt->plot_dir = "benchmark";

	opt->std = 0;
	opt->std_memcache = 0;

	tnt_bench_list_init(&opt->tests, 0);
	tnt_bench_list_init(&opt->bufs, 0);
}

void
tnt_bench_opt_free(tnt_bench_opt_t * opt)
{
	tnt_bench_list_free(&opt->tests);
	tnt_bench_list_free(&opt->bufs);
}
