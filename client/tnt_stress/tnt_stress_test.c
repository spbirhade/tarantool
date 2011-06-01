
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

#include <libtnt.h>

#include <client/tnt_stress/tnt_stress.h>
#include <client/tnt_stress/tnt_stress_test.h>

static void
stress_recv(tnt_t * t, int count)
{
	int key;
	tnt_recv_t rcv; 

	for (key = 0 ; key < count ; key++) {

		tnt_recv_init(&rcv);

		if (tnt_recv(t, &rcv) == -1) {
			printf("recv failed: %s ", tnt_perror(t));

			if (tnt_error(t) == TNT_ESYSTEM)
				printf("(%s)", strerror(tnt_error_errno(t)));

			printf("\n");
		} else
			if (tnt_error(t) != TNT_EOK)
				printf("server respond: %s (op: %d, reqid: %lu, code: %lu, count: %lu)\n",
					tnt_perror(t), TNT_RECV_OP(&rcv),
					TNT_RECV_ID(&rcv),
					TNT_RECV_CODE(&rcv),
					TNT_RECV_COUNT(&rcv));

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
}

void
stress_ping(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	int key;
	long long start = stress_time();

	(void)flags;
	(void)bsize;

	for (key = 0 ; key < count ; key++) {

		if (tnt_ping(t, key) == -1)
			stress_error(t, "ping");
	}

	tnt_flush(t);
	
	stress_recv(t, count);
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

	tnt_tuple_t tu;
	long long start = stress_time();
	int key;

	for (key = 0 ; key < count ; key++) {

		tnt_tuple_init(&tu);
		tnt_tuple_add(&tu, (char*)&key, sizeof(key));

		tnt_tuple_add(&tu, buf, bsize);

		if (tnt_insert(t, key, 0, flags, &tu) == -1)
			stress_error(t, "insert");

		tnt_tuple_free(&tu);
	}

	tnt_flush(t);

	stress_recv(t, count);
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

	tnt_update_t u;
	int key;
	long long start = stress_time();

	for (key = 0 ; key < count ; key++) {

		tnt_update_init(&u);
		tnt_update_add(&u, TNT_UPDATE_ASSIGN, 1, buf, bsize);

		if (tnt_update(t, key, 0, flags, (char*)&key, sizeof(key), &u) == -1)
			stress_error(t, "update");

		tnt_update_free(&u);
	}

	tnt_flush(t);

	stress_recv(t, count);
	stress_end(start, count, stat);

	free(buf);
}

void
stress_select(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	tnt_tuples_t tuples;
	int key;
	long long start = stress_time();

	(void)bsize;
	(void)flags;

	for (key = 0 ; key < count  ; key++) {

		tnt_tuples_init(&tuples);
		tnt_tuple_t * tu = tnt_tuples_add(&tuples);

		tnt_tuple_add(tu, (char*)&key, sizeof(key));

		if (tnt_select(t, key, 0, 0, 0, 100, &tuples) == -1)
			stress_error(t, "select");

		tnt_tuples_free(&tuples);
	}

	tnt_flush(t);

	stress_recv(t, count);
	stress_end(start, count, stat);
}

void
stress_select_set(tnt_t * t,
	int bsize, int count, int flags, stress_stat_t * stat)
{
	tnt_tuples_t tuples;
	int key, c = 0;
	long long start = stress_time();
	int per = 50;

	(void)bsize;
	(void)flags;

	tnt_tuples_init(&tuples);

	for (key = 0 ; key < count  ; key++) {
		
		if (key % per == 0 && (key != 0) ) {

			if (tnt_select(t, key, 0, 0, 0, 50, &tuples) == -1)
				stress_error(t, "select");

			tnt_tuples_free(&tuples);
			tnt_tuples_init(&tuples);
			c++;
		}

		tnt_tuple_t * tu = tnt_tuples_add(&tuples);
		tnt_tuple_add(tu, (char*)&key, sizeof(key));
	}

	tnt_flush(t);

	stress_recv(t, c);
	stress_end(start, c, stat);
}
