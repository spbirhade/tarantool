#ifndef TARANTOOL_AUTH_CHAP_H_INCLUDED
#define TARANTOOL_AUTH_CHAP_H_INCLUDED

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

#define AUTH_CHAP_VERSION      (1)

#define AUTH_CHAP_MAGIC        ("TNT")
#define AUTH_CHAP_MAGIC_SIZE   (3)

#define AUTH_CHAP_TOKEN_SIZE   (8)
#define AUTH_CHAP_ID_SIZE      (8)

/* hash_size must be equal to AES_CMAC_DIGEST_LENGTH */
#define AUTH_CHAP_HASH_SIZE    (16)

struct auth_chap_hdr_server_1 {

	unsigned char magic[AUTH_CHAP_MAGIC_SIZE];
	unsigned char version;
	unsigned char token[AUTH_CHAP_TOKEN_SIZE];
};

#define AUTH_CHAP_RESP_FAIL    (0)
#define AUTH_CHAP_RESP_OK      (1)

struct auth_chap_hdr_server_2 {

	unsigned char magic[AUTH_CHAP_MAGIC_SIZE];
	unsigned char resp;
};

#define AUTH_CHAP_PROTO_ADMIN  (0)
#define AUTH_CHAP_PROTO_RW     (1)
#define AUTH_CHAP_PROTO_RO     (2)
#define AUTH_CHAP_PROTO_FEEDER (3)

struct auth_chap_hdr_client {

	unsigned char magic[AUTH_CHAP_MAGIC_SIZE];
	unsigned char id[AUTH_CHAP_ID_SIZE];
	unsigned char hash[AUTH_CHAP_HASH_SIZE];
	unsigned char proto;
};

#endif
