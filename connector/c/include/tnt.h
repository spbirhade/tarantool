#ifndef TNT_H_
#define TNT_H_

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

#define TNT_TMOUT_DEFAULT (16)

typedef enum {

	TNT_AUTH_NONE,
	TNT_AUTH_CHAP,
	TNT_AUTH_SASL

} tnt_auth_t;

typedef enum {

	TNT_PROTO_ADMIN,
	TNT_PROTO_RW,
	TNT_PROTO_RO,
	TNT_PROTO_FEEDER

} tnt_proto_t;

typedef struct {

	int             connected;
	int             fd;

	char          * sbuf;
	int             sbuf_off;
	int             sbuf_size;

	char          * rbuf;
	int             rbuf_off;
	int             rbuf_top;
	int             rbuf_size;

	int             opt_tmout;
	int             opt_tmout_rcv;
	int             opt_tmout_snd;

	tnt_proto_t     proto;
	tnt_auth_t      auth_type;

	char          * auth_id;
	int             auth_id_size;
	unsigned char * auth_key;
	int             auth_key_size;
	char          * auth_mech;

	tnt_error_t     error;
	int             error_errno;

} tnt_t;

tnt_t*
tnt_init(tnt_proto_t proto, int rbuf_size, int sbuf_size);

void
tnt_set_alloc(tnt_t * t,
	tnt_mallocf_t m, tnt_reallocf_t r, tnt_dupf_t d, tnt_freef_t f);

void
tnt_set_tmout(tnt_t * t, int tmout_connect, int tmout_snd, int tmout_rcv);

int
tnt_set_auth(tnt_t * t, tnt_auth_t auth, char * mech,
	char * id,
	unsigned char * key, int key_size);

void
tnt_free(tnt_t * t);

int
tnt_connect(tnt_t * t, char * hostname, int port);

int
tnt_flush(tnt_t * t);

void
tnt_close(tnt_t * t);

tnt_error_t
tnt_error(tnt_t * t);

int
tnt_error_errno(tnt_t * t);

char*
tnt_perror(tnt_t * t);

#endif
