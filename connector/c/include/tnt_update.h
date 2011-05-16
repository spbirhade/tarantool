#ifndef TNT_UPDATE_H_
#define TNT_UPDATE_H_

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

typedef	enum {

	TNT_UPDATE_NONE,
	TNT_UPDATE_ASSIGN,
	TNT_UPDATE_ADD,
	TNT_UPDATE_AND,
	TNT_UPDATE_XOR,
	TNT_UPDATE_OR

} tnt_update_type_t;

typedef struct _tnt_update_op_t {

	unsigned char op;
	unsigned long field;

	char * data;

	unsigned long size;
	unsigned long size_leb;

	struct _tnt_update_op_t * next;

} tnt_update_op_t;

typedef struct {

	unsigned long count;
	int size_enc;

	tnt_update_op_t * head;
	tnt_update_op_t * tail;

} tnt_update_t;

void
tnt_update_init(tnt_update_t * update);

void
tnt_update_free(tnt_update_t * update);

tnt_error_t
tnt_update_add(tnt_update_t * update,
	tnt_update_type_t type, int field, char * data, int size);

int
tnt_update(tnt_t * t, int reqid, int ns, int flags,
	char * key, int key_size, tnt_update_t * update);

int
tnt_update_tuple(tnt_t * t, int reqid, int ns, int flags,
	tnt_tuple_t * key, tnt_update_t * update);

#endif
