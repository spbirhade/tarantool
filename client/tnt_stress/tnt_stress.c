
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
#include <client/tnt_stress/tnt_stress_test.h>
#include <client/tnt_stress/tnt_stress_memcache.h>

static
stress_t stress_small[] =
{
	{ 1, "ping",    NULL,           0,     0 },
	{ 0, NULL,      stress_ping,    0,     0 },

	{ 1, "insert",  NULL,           0,     0 },
	{ 0, NULL,      stress_insert,  16,    0 },
	{ 0, NULL,      stress_insert,  128,   0 },
	{ 0, NULL,      stress_insert,  256,   0 },

	{ 1, "insert-return", NULL,     0,     0 },
	{ 0, NULL,      stress_insert,  16,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  128,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  256,   TNT_PROTO_FLAG_RETURN },

	{ 1, "update",  NULL,           0,     0 },
	{ 0, NULL,      stress_update,  16,    0 },
	{ 0, NULL,      stress_update,  128,   0 },
	{ 0, NULL,      stress_update,  256,   0 },

	{ 1, "update-return", NULL,     0,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  16,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  128,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  256,   TNT_PROTO_FLAG_RETURN },

	{ 1, "select", NULL,            0,     0 },
	{ 0, NULL,      stress_select,  0,     0 },

	{ 1, "select-set", NULL,        0,     0 },
	{ 0, NULL,      stress_select_set, 0,  0 },

	{ 2, NULL,      0,              0,     0 }
};

static
stress_t stress_memcache[] =
{
	{ 1, "set", NULL, 0, 0 },
	{ 0, NULL,  stress_memcache_set, 100, 0 },

	{ 1, "get", NULL, 0, 0 },
	{ 0, NULL,  stress_memcache_get, 100, 0 },

	{ 2, NULL,  0,    0, 0 }
};

static
stress_t stress_memcache_iproto[] =
{
	{ 1, "insert",  NULL,           0,   0 },
	{ 0, NULL,      stress_insert,  100, 0 },

	{ 1, "select",  NULL,           0,   0 },
	{ 0, NULL,      stress_select,  0,   0 },

	{ 2, NULL,  0,    0, 0 }
};

void
stress_error(tnt_t * t, char * name)
{
	printf("%s failed: %s", name, tnt_perror(t));

	if (tnt_error(t) == TNT_ESYSTEM)
		printf("(%s)", strerror(tnt_error_errno(t)));

	printf("\n");
}

long long
stress_time(void)
{
    long long tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    tm = ((long)tv.tv_sec)*1000;
    tm += tv.tv_usec/1000;

    return tm;
}

void
stress_end(long long start, int count, stress_stat_t * stat)
{
	stat->tm  = stress_time() - start;
	stat->rps = (float)count / ((float)stat->tm / 1000);
}

static void
stress_run(tnt_t * t, stress_t * list,
	int color, int compact, int count, int reps)
{
	stress_stat_t * stats = malloc(sizeof(stress_stat_t) * reps);

	if (stats == NULL)
		return;

	int i;

	for (i = 0 ; list[i].group != 2 ; i++) {

		stress_t * s = &list[i];

		if (s->group) {
			
			if (color)
				printf("[\033[22;33m%s\033[0m]\n", s->name);
			else
				printf("[%s]\n", s->name);
			continue;
		}

		if (compact)
			printf("  [%3d]", s->bsize);
		else {
			if (s->name) 
				printf("  -> %s (bsize: %d bytes, flags: %d)\n",
					s->name, s->bsize, s->flags);
			else
				printf("  -> %d bytes\n", s->bsize);
			printf("  ");
		}

		memset(stats, 0, sizeof(stress_stat_t) * reps);

		int r;

		for (r = 0 ; r < reps ; r++) {

			stats[r].ptr = s;
			s->f(t, s->bsize, count, s->flags, &stats[r]);

			if (!compact)
				printf("[%.2f %.2f] ", stats[r].rps, (float)stats[r].tm / 1000);

			fflush(stdout);
		}

		unsigned long long tm = 0;
		float rps = 0.0, avg, avgtm;

		for (r = 0 ; r < reps ; r++) {

			rps += stats[r].rps;
			tm += stats[r].tm;
		}

		avgtm = (float)tm / 1000 / reps;
		avg = rps / reps;

		if (compact) {

			if (color)
				printf(" \033[22;32m%.2f\033[0m\n", avg);
			else
				printf(" %.2f\n", avg);
		} else {
			printf("\n");

			if (color)
				printf("  >> (avg time \033[22;35m%.2f\033[0m seconds): \033[22;32m%.2f\033[0m avg rps\n", 
					avgtm, avg);
			else
				printf("  >> (avg time %.2f seconds): %.2f avg rps\n", 
					avgtm, avg);
		}
	}

	free(stats);
}

static void
usage(void)
{
	printf("tarantool stress-suite.\n\n");

	printf("tnt_stress [options]\n");

	printf("connection:\n");
	printf("  -a [host]        server address {localhost}\n");
	printf("  -p [port]        server port {15312}\n");
	printf("  -r [rbuf]        receive buffer size {16384}\n");
	printf("  -s [sbuf]        send buffer size {16384}\n");
	printf("  -M               memcache mode {off}\n");
	printf("  -K               memcache test for iproto {off}\n\n");

	printf("authentication:\n");
	printf("  -t [auth_type]   authentication type {chap, sasl}\n");
	printf("  -i [id]          authentication id {test}\n");
	printf("  -k [key]         authentication key {test}\n");
	printf("  -m [mech]        authentication SASL mechanism {PLAIN}\n\n");

	printf("stress:\n");
	printf("  -C [count]       request count {1000}\n");
	printf("  -R [rep]         count of request repeats {1000}\n");
	printf("  -c               compact mode {off}\n");
	printf("  -b [color]       color output {on}\n");
}

int
main(int argc, char * argv[])
{
	tnt_auth_t auth = TNT_AUTH_CHAP;

	char * host = "localhost";
	int port = 15312;

	char * id = "test";
	char * key = "1234567812345678";
	int key_size = 16;
	char * mech = "PLAIN";

	int color = 1;
	int count = 1000;
	int rbuf = 16384;
	int sbuf = 16384;

	int reps = 1;
	int compact = 0;
	int memcache = 0;
	int memcache_set = 0;
	int opt;

	while ((opt = getopt(argc, argv, "t:i:k:m:a:p:cMR:C:r:s:hb:K")) != -1 ) {
		switch (opt) {
			case 't':
				if (!strcmp(optarg, "chap"))
					auth = TNT_AUTH_CHAP;
				else
				if (!strcmp(optarg, "sasl"))
					auth = TNT_AUTH_SASL;
				else
					auth = TNT_AUTH_NONE;
				break;

			case 'i':
				id = optarg;
				break;

			case 'k':
				key = optarg;
				key_size = strlen(optarg);
				break;

			case 'm':
				mech = optarg;
				break;

			case 'a':
				host = optarg;
				break;

			case 'p':
				port = atoi(optarg);
				break;

			case 'b':
				color = atoi(optarg);
				break;

			case 'R':
				reps = atoi(optarg);
				break;

			case 'C':
				count = atoi(optarg);
				break;

			case 'c':
				compact = 1;
				break;

			case 'r':
				rbuf = atoi(optarg);
				break;

			case 's':
				sbuf = atoi(optarg);
				break;

			case 'M':
				memcache = 1;
				break;

			case 'K':
				memcache_set = 1;
				break;

			case 'h':
			default:
				usage();
				return 1;
		}
	}

	printf("tarantool stress-suite.\n\n");

	if (!compact) {

		printf("server: %s (%d)\n", host, port);
		printf("id:     %s\n", id);
		printf("key:    %s\n", key);
		printf("rbuf:   %d\n", rbuf);
		printf("sbuf:   %d\n", sbuf);
		printf("count:  %d\n", count);
		printf("reps:   %d\n\n", reps);
	}

	tnt_t * t = tnt_init(TNT_PROTO_RW, rbuf, sbuf);

	if (t == NULL) {

		printf("tnt_init() failed\n");
		return 1;
	}

	tnt_set_tmout(t, 8, 1, 1);

	if (tnt_set_auth(t, auth, mech,
			id,
			(unsigned char*)key, key_size) == -1) {

		printf("tnt_set_auth() failed: %s ", tnt_perror(t));

		if (tnt_error(t) == TNT_ESYSTEM)
			printf("(%s)", strerror(tnt_error_errno(t)));

		printf("\n");
		return 1;
	}

	if (tnt_connect(t, host, port) == -1) {

		printf("tnt_connect() failed: %s ", tnt_perror(t));

		if (tnt_error(t) == TNT_ESYSTEM)
			printf("(%s)", strerror(tnt_error_errno(t)));

		printf("\n");
		return 1;
	}

	if (memcache)
		stress_run(t, stress_memcache, color, compact, count, reps);
	else {
		if (memcache_set)
			stress_run(t, stress_memcache_iproto, color, compact, count, reps);
		else
			stress_run(t, stress_small, color, compact, count, reps);
	}

	tnt_free(t);
	return 0;
}
