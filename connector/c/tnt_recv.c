
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/uio.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_opt.h>
#include <tnt_buf.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_tuple.h>
#include <tnt_proto.h>
#include <tnt_recv.h>

void
tnt_recv_init(tnt_recv_t * rcv)
{
	rcv->count = 0;
	rcv->reqid = 0;
	rcv->code = 0;
	tnt_tuples_init(&rcv->tuples);
}

void
tnt_recv_free(tnt_recv_t * rcv)
{
	tnt_tuples_free(&rcv->tuples);
}

char*
tnt_recv_error(tnt_recv_t * rcv)
{
	(void)rcv;
	return NULL;
}

static tnt_error_t
tnt_recv_fqtuple(tnt_recv_t * rcv, char * data, unsigned long size,
	unsigned long count)
{
	char * p = data;
	unsigned long i, off = 0;
	for (i = 0 ; i < count ; i++) {
		unsigned long s = *(unsigned long*)p;
		if (s > (unsigned long)(size - off)) {
			tnt_tuples_free(&rcv->tuples);
			return TNT_EPROTO;
		}
		off += 4, p += 4;
		s   += 4;
		tnt_error_t r = tnt_tuples_unpack(&rcv->tuples, p, s);
		if (r != TNT_EOK) {
			tnt_tuples_free(&rcv->tuples);
			return r;
		}
		off += s, p += s;
	}

	return TNT_EOK;
}

int
tnt_recv(tnt_t * t, tnt_recv_t * rcv)
{
	char buffer[sizeof(tnt_proto_header_resp_t) + 4];
	t->error = tnt_io_recv(t, buffer, sizeof(tnt_proto_header_t));
	if (t->error != TNT_EOK)
		return -1;

	tnt_proto_header_resp_t * hdr =
		(tnt_proto_header_resp_t*)buffer;

	rcv->reqid = hdr->hdr.reqid;
	rcv->code = 0;
	rcv->count = 0;

	switch (hdr->hdr.type) {
	case TNT_PROTO_TYPE_PING:
		rcv->op = TNT_RECV_PING;
		return TNT_EOK;
	case TNT_PROTO_TYPE_INSERT:
		rcv->op = TNT_RECV_INSERT;
		break;
	case TNT_PROTO_TYPE_UPDATE:
		rcv->op = TNT_RECV_UPDATE;
		break;
	case TNT_PROTO_TYPE_DELETE:
		rcv->op = TNT_RECV_DELETE;
		break;
	case TNT_PROTO_TYPE_SELECT:
		rcv->op = TNT_RECV_SELECT;
		break;
	default:
		return TNT_EPROTO;
	}

	/* BOX_QUIET flag or error/reply req */
	if (hdr->hdr.len == 4) {
		t->error = tnt_io_recv(t, buffer + sizeof(tnt_proto_header_t),
				sizeof(tnt_proto_header_resp_t) -
				sizeof(tnt_proto_header_t));
		if (t->error != TNT_EOK)
			return -1;
		rcv->code = hdr->code;
		if (TNT_PROTO_IS_ERROR(hdr->code)) {
			t->error = TNT_EERROR;
			return 0;
		} else
		if (TNT_PROTO_IS_AGAIN(hdr->code)) {
			t->error = TNT_EAGAIN;
			return 0;
		}
		return 0;
	}

	if ((rcv->op != TNT_RECV_SELECT) && (hdr->hdr.len == 8)) {
		/* count only - insert, update, delete */
		t->error = tnt_io_recv(t, buffer + sizeof(tnt_proto_header_t),
				sizeof(tnt_proto_header_resp_t) + 4 -
				sizeof(tnt_proto_header_t));
		if (t->error != TNT_EOK)
			return -1;
		rcv->count = *(unsigned long*)(buffer +
			sizeof(tnt_proto_header_resp_t));
		return 0;
	} 

	int size = hdr->hdr.len;
	char * data = tnt_mem_alloc(size);
	if (data == NULL) {
		t->error = TNT_EMEMORY;
		return -1;
	}
	char * p = data;
	t->error = tnt_io_recv(t, p, size);
	if (t->error != TNT_EOK) {
		tnt_mem_free(data);
		return -1;
	}
	hdr->code = *(unsigned long*)(p); 
	p += sizeof(unsigned long);
	size -= 4;
	rcv->count = *(unsigned long*)(p);
	p += sizeof(unsigned long);
	size -= 4;

	switch (rcv->op) {
	/* <insert_response_body> ::= <count> | <count><fq_tuple> 
	   <update_response_body> ::= <insert_response_body>
	*/
	case TNT_RECV_INSERT:
	case TNT_RECV_UPDATE:
		t->error = tnt_recv_fqtuple(rcv, p, size, 1);
		break;
	/* <delete_response_body> ::= <count> */
	case TNT_RECV_DELETE:
		/* unreach */
		break;
	/* <select_response_body> ::= <count><fq_tuple>* */
	case TNT_RECV_SELECT:
		/* fq_tuple* */
		t->error = tnt_recv_fqtuple(rcv, p, size, rcv->count);
		break;
	default:
		t->error = TNT_EPROTO;
		break;
	}
	if (data)
		tnt_mem_free(data);
	return (t->error == TNT_EOK) ? 0 : -1;
}
