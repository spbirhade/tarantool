#ifndef TARANTOOL_AUTH_H_INCLUDED
#define TARANTOOL_AUTH_H_INCLUDED

/*
 * Copyright (C) 2010 Mail.RU
 * Copyright (C) 2010 Yuriy Vostrikov
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

enum auth_result {

	AUTH_FAIL,

	AUTH_PROTO_ADMIN,
	AUTH_PROTO_RW,
	AUTH_PROTO_RO,
	AUTH_PROTO_FEEDER
};

typedef bool (*auth_initf)(void);
typedef void (*auth_freef)(void);

typedef enum auth_result (*auth_handshakef)(struct user ** u);

struct auth_mech {

	int id;

	char * name;
	char * desc;

	auth_freef free;
	auth_handshakef handshake;
	
	SLIST_ENTRY(auth_mech) next;
};

struct auth {

	int id;
	int count;

	SLIST_HEAD(, auth_mech) list;
};

void
auth_init(void);

void
auth_free(void);

int
auth_add(char * name, char * desc,
	auth_initf init, auth_freef ff, auth_handshakef hs);

bool
auth_del(int id);

struct auth_mech*
auth_match_id(int id);

struct auth_mech*
auth_match_name(char * name);

void
auth_print(void);

#endif
