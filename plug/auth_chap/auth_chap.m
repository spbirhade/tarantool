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
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <tarantool.h>

#include <third_party/queue.h>
#include <third_party/rijndael.h>
#include <third_party/cmac.h>

#include <fiber.h>
#include <say.h>
#include <user.h>
#include <plug.h>
#include <auth.h>
#include <auth_chap.h>

#include <plug/auth_chap/auth_chap.h>

extern pid_t master_pid;
extern struct users users;

static bool
auth_chap_init(void)
{
	struct user * u;

	SLIST_FOREACH(u, &users.list, next) {

		if (u->key_size != AES_CMAC_KEY_LENGTH) {

			say_warn("[auth_chap]: user \"%s\" has incompatible key size %d, must be %d",
					u->id, u->key_size, AES_CMAC_KEY_LENGTH);
			return false;
		}
	}

	return true;
}

static bool
auth_chap_hashcmp(struct user * u,
	struct auth_chap_hdr_server_1 * hs1,
	struct auth_chap_hdr_client * hc)
{
	AES_CMAC_CTX cmac;
	u_int8_t hash[AES_CMAC_DIGEST_LENGTH];

	if (u->key_size != AES_CMAC_KEY_LENGTH)
		return false;

	AES_CMAC_Init(&cmac);

	/* key size must be equal to AES_CMAC_KEY_LENGTH */
	AES_CMAC_SetKey(&cmac, u->key);

	AES_CMAC_Update(&cmac, (u_int8_t*)hs1->token, sizeof(hs1->token));
	AES_CMAC_Final(hash, &cmac);

	if (memcmp(hc->hash, hash, AES_CMAC_DIGEST_LENGTH))
		return false;
	return true;
}

static enum auth_result
auth_chap_handshake_ret(enum auth_result r)
{
	struct auth_chap_hdr_server_2 hs2;

	memcpy(hs2.magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE);

	if ( r == AUTH_FAIL )
		hs2.resp = AUTH_CHAP_RESP_FAIL;
	else
		hs2.resp = AUTH_CHAP_RESP_OK;

	add_iov((void*)&hs2, sizeof(hs2));

	return (fiber_flush_output() < 0) ? AUTH_FAIL : r;
}

static enum auth_result
auth_chap_handshake(struct user ** u)
{
	/* Stage1. Server sends random value (Token) */

	struct auth_chap_hdr_server_1 hs1;

	memcpy(hs1.magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE);
	hs1.version = AUTH_CHAP_VERSION;

	/* randomizing token */

	unsigned long long tk;
	time_t tm;

	time(&tm);

	tk ^= rand() + fiber->fid + fiber->fd;
	tk ^= tm + master_pid;

	memcpy(hs1.token, (void*)&tk, sizeof(tk));

	add_iov((void*)&hs1, sizeof(hs1));

	ssize_t r = fiber_flush_output();

	if (r < 0)
		return AUTH_FAIL;

	/* Stage2. Client sends signed value (Hash) and Protocol Type */

	if (fiber_bread(fiber->rbuf, sizeof(struct auth_chap_hdr_server_1)) <= 0)
		return auth_chap_handshake_ret(AUTH_FAIL);

	if (fiber->rbuf->len < sizeof(struct auth_chap_hdr_client))
		return auth_chap_handshake_ret(AUTH_FAIL);

	/* Stage3. Server authenticates client and send response */

	struct auth_chap_hdr_client * hc =
		(struct auth_chap_hdr_client*)fiber->rbuf->data;

	/* magic, proto and user check */

	if (memcmp(hc->magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE))
		return auth_chap_handshake_ret(AUTH_FAIL);

	hc->id[AUTH_CHAP_ID_SIZE-1] = 0;

	*u = user_match((char*)hc->id);

	if (*u == NULL)
		return auth_chap_handshake_ret(AUTH_FAIL);

	/* checking signature */

	if (!auth_chap_hashcmp(*u, &hs1, hc))
		return auth_chap_handshake_ret(AUTH_FAIL);

	/* - - - */

	enum auth_result ar;

	switch (hc->proto) {

		case AUTH_CHAP_PROTO_ADMIN:
			ar = AUTH_PROTO_ADMIN;
			break;

		case AUTH_CHAP_PROTO_FEEDER:
			ar = AUTH_PROTO_FEEDER;
			break;

		case AUTH_CHAP_PROTO_RW:
			ar = AUTH_PROTO_RW;
			break;

		case AUTH_CHAP_PROTO_RO:
			ar = AUTH_PROTO_RO;
			break;

		default:
			ar = AUTH_FAIL;
			break;
	}

	return auth_chap_handshake_ret(ar);
}

static
struct plug_desc auth_chap_desc = {

	.name   = "auth_chap",
	.author = "pmwka",
	.desc   = "tarantool native authentication plugin"
};

int
tnt_module_init(struct plug_desc **pd)
{
	*pd = &auth_chap_desc;

	int result =
		auth_add("chap", "CHAP authentication protocol",
			auth_chap_init,
			NULL,
			(auth_handshakef)auth_chap_handshake);

	if (result == -1)
		return TNT_MODULE_FAIL;

	return TNT_MODULE_OK;
}
