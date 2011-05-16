
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
#include <tnt_select.h>

int
tnt_select(tnt_t * t, int reqid, int ns, int index, int offset,
	int limit, tnt_tuples_t * tuples)
{
	char * data_enc;
	int data_enc_size;

	t->error = tnt_tuples_pack(tuples, &data_enc, &data_enc_size);

	if (t->error != TNT_EOK)
		return -1;

	tnt_proto_header_t hdr;

	hdr.type  = TNT_PROTO_TYPE_SELECT;
	hdr.len   = sizeof(tnt_proto_select_t) + data_enc_size;
	hdr.reqid = reqid;

	tnt_proto_select_t hdr_sel;

	hdr_sel.ns     = ns;
	hdr_sel.index  = index;
	hdr_sel.offset = offset;
	hdr_sel.limit  = limit;

	struct iovec v[3];

	v[0].iov_base = &hdr;
	v[0].iov_len  = sizeof(tnt_proto_header_t);

	v[1].iov_base = &hdr_sel;
	v[1].iov_len  = sizeof(tnt_proto_select_t);

	v[2].iov_base = data_enc;
	v[2].iov_len  = data_enc_size;

	t->error = tnt_io_sendv(t, v, 3);

	tnt_mem_free(data_enc);
	return (t->error == TNT_EOK) ? 0 : -1;
}
