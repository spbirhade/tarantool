
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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include <libtnt.h>

#include <client/tnt_stress/tnt_stress.h>
#include <client/tnt_stress/tnt_stress_memcache.h>

void
stress_memcache_set(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	(void)flags;

	char * buf = malloc(bsize);
	if (buf == NULL) {
		printf("memory allocation of %d bytes failed\n", bsize);
		return;
	}

	memset(buf, 'x', bsize);

	int key;
	long long start = stress_time();

	for (key = 0 ; key < count ; key++) {

		char keydesc[32];
		snprintf(keydesc, sizeof(keydesc), "key_%d", key);

		if (tnt_memcache_set(t, 0, 0, keydesc, buf, bsize) == -1)
			stress_error(t, "set");
	}

	stress_end(start, count, stat);
	free(buf);
}

void
stress_memcache_get(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	(void)bsize;
	(void)flags;

	int key;
	long long start = stress_time();

	char keydesc[32];
	char * keyptr[1] = { keydesc };

	for (key = 0 ; key < count ; key++) {

		snprintf(keydesc, sizeof(keydesc), "key_%d", key);

		tnt_memcache_vals_t vals;
		tnt_memcache_val_init(&vals);

		if (tnt_memcache_get(t, false, 1, keyptr, &vals) == -1)
			stress_error(t, "get");

		tnt_memcache_val_free(&vals);
	}

	stress_end(start, count, stat);
}
