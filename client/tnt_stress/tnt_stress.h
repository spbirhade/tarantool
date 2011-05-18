#ifndef TARANTOOL_TEST_STRESS_H_INCLUDED
#define TARANTOOL_TEST_STRESS_H_INCLUDED

/*
 * Copyright (C) 2010 Mail.RU
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

typedef struct {

	long long tm;
	float rps;

} stress_stat_t;

typedef void (*stressf_t)(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

typedef struct {

	int         group;
	char      * name;
	stressf_t   f;
	int         bsize;
	int         flags;

} stress_t;

void
stress_ping(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

void
stress_insert(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

void
stress_update(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

void
stress_select(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

void
stress_select_set(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat);

#endif
