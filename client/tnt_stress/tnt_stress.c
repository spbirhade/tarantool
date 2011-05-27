
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libtnt.h>
#include <client/tnt_stress/tnt_stress.h>

static
stress_t stress_big[] =
{
	{ 1, "ping",    NULL,           0,     0 },
	{ 0, NULL,      stress_ping,    0,     0 },

	{ 1, "insert",  NULL,           0,     0 },
	{ 0, NULL,      stress_insert,  2,     0 },
	{ 0, NULL,      stress_insert,  8,     0 },
	{ 0, NULL,      stress_insert,  32,    0 },
	{ 0, NULL,      stress_insert,  64,    0 },
	{ 0, NULL,      stress_insert,  128,   0 },
	{ 0, NULL,      stress_insert,  256,   0 },
	{ 0, NULL,      stress_insert,  512,   0 },

	{ 1, "insert-return", NULL,     0,     0 },
	{ 0, NULL,      stress_insert,  2,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  8,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  32,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  64,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  128,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  256,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_insert,  512,   TNT_PROTO_FLAG_RETURN },

	{ 1, "update",  NULL,           0,     0 },
	{ 0, NULL,      stress_update,  2,     0 },
	{ 0, NULL,      stress_update,  8,     0 },
	{ 0, NULL,      stress_update,  32,    0 },
	{ 0, NULL,      stress_update,  64,    0 },
	{ 0, NULL,      stress_update,  128,   0 },
	{ 0, NULL,      stress_update,  256,   0 },
	{ 0, NULL,      stress_update,  512,   0 },

	{ 1, "update-return", NULL,     0,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  2,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  8,     TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  32,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  64,    TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  128,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  256,   TNT_PROTO_FLAG_RETURN },
	{ 0, NULL,      stress_update,  512,   TNT_PROTO_FLAG_RETURN },

	{ 1, "select", NULL,            0,     0 },
	{ 0, NULL,      stress_select,  0,     0 },

	{ 1, "select-set", NULL,        0,     0 },
	{ 0, NULL,      stress_select_set, 0,  0 },

	{ 2, NULL,      0,              0,     0 }
};

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

int
main(int argc, char * argv[])
{
	tnt_auth_t auth = TNT_AUTH_CHAP;

	char * host = "localhost";
	int port = 15312;

	char * id = "test";
	char * key = "1234567812345678";
	int key_size = 16;
	char * mech = "SCRAM-SHA-1";

	int count = 1000;
	int rbuf = 16384;
	int sbuf = 16384;

	int compact = 0;
	int reps = 1;
	int color = 1;
	int big = 0;
	int opt;

	while ((opt = getopt(argc, argv, "t:i:k:m:a:p:c:B:R:Cr:s:hb")) != -1 ) {
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

			case 'c':
				count = atoi(optarg);
				break;

			case 'B':
				color = atoi(optarg);
				break;

			case 'C':
				compact = 1;
				break;

			case 'R':
				reps = atoi(optarg);
				break;

			case 'r':
				rbuf = atoi(optarg);
				break;

			case 's':
				sbuf = atoi(optarg);
				break;

			case 'b':
				big = 1;
				break;

			case 'h':
			default:
				printf("tarantool stress-suite.\n\n");

				printf("stress: [-t auth  = chap, sasl]\n"
				       "        [-i id    = test]\n"
				       "        [-k key   = 1234567812345678]\n"
				       "        [-m mech  = PLAIN]\n"
			 	       "        [-a host  = localhost]\n"
				       "        [-p port  = 15312]\n"
				       "        [-c count = 1000]\n"
				       "        [-B color = 1]\n"
				       "        [-R reps  = 1]\n"
				       "        [-C compact = 0]\n"
				       "        [-r rbuf  = 16384]\n"
				       "        [-s sbuf  = 16384]\n"
				       "        [-b       = 0]\n");

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

	if (big)
		stress_run(t, stress_big, color, compact, count, reps);
	else
		stress_run(t, stress_small, color, compact, count, reps);

	tnt_free(t);
	return 0;
}
