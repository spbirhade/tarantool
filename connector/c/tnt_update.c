
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL TNT_AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_tuple.h>
#include <tnt_proto.h>
#include <tnt_leb128.h>
#include <tnt_update.h>

void
tnt_update_init(tnt_update_t * update)
{
	update->count    = 0;
	update->size_enc = 0;
	update->head     = NULL;
	update->tail     = NULL;
}

void
tnt_update_free(tnt_update_t * update)
{
	tnt_update_op_t * op, * next;

	for (op = update->head ; op ; op = next) {

		next = op->next;

		tnt_mem_free(op->data);
		tnt_mem_free(op);
	}
}

tnt_error_t
tnt_update_add(tnt_update_t * update,
	tnt_update_type_t type, int field, char * data, int size)
{
	tnt_update_op_t * op = tnt_mem_alloc(sizeof(tnt_update_op_t));

	if (op == NULL)
		return TNT_EMEMORY;

	if (update->head == NULL)
		update->head = op;
	else
		update->tail->next = op;

	update->tail = op;
	update->count++;

	switch (type) {

		case TNT_UPDATE_ASSIGN:
			op->op = TNT_PROTO_UPDATE_ASSIGN;
			break;

		case TNT_UPDATE_ADD:
			op->op = TNT_PROTO_UPDATE_ADD;
			break;

		case TNT_UPDATE_AND:
			op->op = TNT_PROTO_UPDATE_AND;
			break;

		case TNT_UPDATE_XOR:
			op->op = TNT_PROTO_UPDATE_XOR;
			break;
			
		case TNT_UPDATE_OR:
			op->op = TNT_PROTO_UPDATE_OR;
			break;

		default:
			tnt_mem_free(op);
			return TNT_EFAIL;
	}

	op->field = field;

	op->size  = size;
	op->data  = tnt_mem_alloc(size);
	op->next  = NULL;

	if (op->data == NULL ) {
		tnt_mem_free(op);
		return TNT_EMEMORY;
	}

	op->size_leb = tnt_leb128_size(size);
	update->size_enc += 4 + 1 + op->size_leb + op->size;

	memcpy(op->data, data, op->size);
	return TNT_EOK;
}

static tnt_error_t
tnt_update_pack(tnt_update_t * update, char ** data, int * size)
{
	tnt_update_op_t * op;
	char * p;

	if (update->count == 0)
		return TNT_ENOOP;

	/* <count><operation>+ */

	*size = 4 + update->size_enc;
	*data = tnt_mem_alloc(*size);

	if (*data == NULL)
		return TNT_EMEMORY;

	p = *data;

	memcpy(p, &update->count, sizeof(unsigned long));
	p += 4;

	/*  <operation> ::= <field_no><op_code><op_arg>
		
		<field_no>  ::= <int32>
		<op_code>   ::= <int8> 
		<op_arg>    ::= <field>
		<field>     ::= <int32_varint><data>
		<data>      ::= <int8>+
	*/

	for (op = update->head ; op ; op = op->next) {

		memcpy(p, (void*)&op->field, sizeof(unsigned long));
		p += sizeof(unsigned long);

		memcpy(p, (void*)&op->op, sizeof(unsigned char));
		p += sizeof(unsigned char);

		tnt_leb128_write(p, op->size);
		p += op->size_leb;

		memcpy(p, op->data, op->size); 
		p += op->size;
	}

	return TNT_EOK;
}

int
tnt_update_tuple(tnt_t * t, int reqid, int ns, int flags,
	tnt_tuple_t * key, tnt_update_t * update)
{
	char * td;
	int ts;

	t->error = tnt_tuple_pack(key, &td, &ts);

	if (t->error != TNT_EOK)
		return -1;

	char * ud;
	int us;

	t->error = tnt_update_pack(update, &ud, &us);

	if (t->error != TNT_EOK) {
		tnt_mem_free(td);
		return -1;
	}

	tnt_proto_header_t hdr;

	hdr.type  = TNT_PROTO_TYPE_UPDATE;
	hdr.len   = sizeof(tnt_proto_update_t) + ts + us;
	hdr.reqid = reqid;

	tnt_proto_update_t hdr_update;

	hdr_update.ns = ns;
	hdr_update.flags = flags;

	struct iovec v[4];

	v[0].iov_base = &hdr;
	v[0].iov_len  = sizeof(tnt_proto_header_t);
	v[1].iov_base = &hdr_update;
	v[1].iov_len  = sizeof(tnt_proto_update_t);
	v[2].iov_base = td;
	v[2].iov_len  = ts;
	v[3].iov_base = ud;
	v[3].iov_len  = us;

	t->error = tnt_io_sendv(t, v, 4);

	tnt_mem_free(td);
	tnt_mem_free(ud);
	return (t->error == TNT_EOK) ? 0 : -1;
}

int
tnt_update(tnt_t * t, int reqid, int ns, int flags,
	char * key, int key_size, tnt_update_t * update)
{
	tnt_tuple_t k;
	tnt_tuple_init(&k);

	t->error = tnt_tuple_add(&k, key, key_size);

	if (t->error != TNT_EOK)
		return -1;

	int result = tnt_update_tuple(t, reqid, ns, flags, &k, update);

	tnt_tuple_free(&k);
	return result;
}
