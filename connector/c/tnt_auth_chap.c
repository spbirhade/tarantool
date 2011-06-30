
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

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_opt.h>
#include <tnt_buf.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_aes.h>
#include <tnt_cmac.h>
#include <tnt_auth_chap.h>

static void
tnt_auth_chap_hash(tnt_t * t, tnt_auth_chap_hdr_server1_t * hs1,
	tnt_auth_chap_hdr_client_t * hc)
{
	tnt_aes_cmac_ctx_t cmac;
	tnt_aes_cmac_init(&cmac);
	tnt_aes_cmac_setkey(&cmac, t->opt.auth_key);
	tnt_aes_cmac_update(&cmac, (u_int8_t*)hs1->token, sizeof(hs1->token));
	tnt_aes_cmac_final((u_int8_t*)hc->hash, &cmac);
}

tnt_error_t
tnt_auth_chap(tnt_t * t)
{
	/* Stage1. Recv server Token */
	tnt_auth_chap_hdr_server1_t hs1;
	tnt_error_t r = tnt_io_recv(t,
		(void*)&hs1, sizeof(tnt_auth_chap_hdr_server1_t));
	if (r != TNT_EOK)
		return r;
	if (memcmp(hs1.magic, TNT_AUTH_CHAP_MAGIC, TNT_AUTH_CHAP_MAGIC_SIZE))
		return TNT_EPROTO;

	/* Stage2. Creating hash by signing Token */
	tnt_auth_chap_hdr_client_t hc;
	switch (t->opt.proto) {
	case TNT_PROTO_ADMIN:
		hc.proto = TNT_AUTH_CHAP_PROTO_ADMIN;
		break;
	case TNT_PROTO_RW:
		hc.proto = TNT_AUTH_CHAP_PROTO_RW;
		break;
	case TNT_PROTO_RO:
		hc.proto = TNT_AUTH_CHAP_PROTO_RO;
		break;
	case TNT_PROTO_FEEDER:
		hc.proto = TNT_AUTH_CHAP_PROTO_FEEDER;
		break;
	}
	memcpy(hc.magic, TNT_AUTH_CHAP_MAGIC, TNT_AUTH_CHAP_MAGIC_SIZE);

	memset(hc.id, 0, sizeof(hc.id));
	memcpy(hc.id, t->opt.auth_id, t->opt.auth_id_size);

	tnt_auth_chap_hash(t, &hs1, &hc);
	r = tnt_io_send(t, (void*)&hc, sizeof(tnt_auth_chap_hdr_client_t));
	if (r != TNT_EOK)
		return r;

	/* Stage3. Check response */
	tnt_auth_chap_hdr_server2_t hs2;
	r = tnt_io_recv(t, (void*)&hs2, sizeof(tnt_auth_chap_hdr_server2_t));
	if (r != TNT_EOK)
		return r;
	if (memcmp(hs2.magic, TNT_AUTH_CHAP_MAGIC, TNT_AUTH_CHAP_MAGIC_SIZE))
		return TNT_EPROTO;
	if (hs2.resp == TNT_AUTH_CHAP_RESP_OK)
		return TNT_EOK;

	return TNT_EAUTH;
}
