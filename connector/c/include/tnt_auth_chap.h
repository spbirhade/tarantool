#ifndef TNT_AUTH_CHAP_H_
#define TNT_AUTH_CHAP_H_

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

#define TNT_AUTH_CHAP_VERSION     (1)
#define TNT_AUTH_CHAP_MAGIC       ("TNT")
#define TNT_AUTH_CHAP_MAGIC_SIZE  (3)
#define TNT_AUTH_CHAP_TOKEN_SIZE  (8)
#define TNT_AUTH_CHAP_ID_SIZE     (8)
#define TNT_AUTH_CHAP_HASH_SIZE   (16)

typedef struct {

	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char version;
	unsigned char token[TNT_AUTH_CHAP_TOKEN_SIZE];

} tnt_auth_chap_hdr_server1_t;

#define TNT_AUTH_CHAP_RESP_FAIL   (0)
#define TNT_AUTH_CHAP_RESP_OK     (1)

typedef struct {

	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char resp;

} tnt_auth_chap_hdr_server2_t;

#define TNT_AUTH_CHAP_PROTO_ADMIN  (0)
#define TNT_AUTH_CHAP_PROTO_RW     (1)
#define TNT_AUTH_CHAP_PROTO_RO     (2)
#define TNT_AUTH_CHAP_PROTO_FEEDER (3)

typedef struct tnt_auth_chap_hdr_client_t {

	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char id[TNT_AUTH_CHAP_ID_SIZE];
	unsigned char hash[TNT_AUTH_CHAP_HASH_SIZE];
	unsigned char proto;

} tnt_auth_chap_hdr_client_t;

tnt_error_t
tnt_auth_chap(tnt_t * t);

#endif
