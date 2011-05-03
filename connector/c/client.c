/*
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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <tarantool.h>

#include <third_party/queue.h>
#include <third_party/rijndael.h>
#include <third_party/cmac.h>

#include <connector/c/client.h>
#include <auth_chap.h>

struct tnt*
tnt_init(void)
{
	struct tnt * tnt = malloc(sizeof(struct tnt));

	if (tnt == NULL)
		return NULL;

	memset(tnt, 0, sizeof(struct tnt));

	tnt->auth_type = TNT_AUTH_NONE;
	return tnt;
}

enum tnt_result
tnt_init_auth(struct tnt * tnt, enum tnt_auth auth,
	enum tnt_proto proto, char * id, char * key, int key_size)
{
	tnt->auth_type = auth;

	switch (tnt->auth_type) {

		case TNT_AUTH_NONE:
			return TNT_EOK;

		case TNT_AUTH_CHAP:

			if ( tnt->auth_key_size > AES_CMAC_KEY_LENGTH )
				return TNT_EBADVAL;
			break;
	}

	tnt->auth_proto = proto;
	tnt->auth_id_size = strlen(id);

	if ( (tnt->auth_id_size + 1) > AUTH_CHAP_ID_SIZE )
		return TNT_EBADVAL;

	tnt->auth_id = malloc(tnt->auth_id_size);

	if (tnt->auth_id == NULL)
		return TNT_EMEMORY;

	memcpy(tnt->auth_id, id, tnt->auth_id_size);

	tnt->auth_key_size = key_size;
	tnt->auth_key = malloc(tnt->auth_key_size);

	if (tnt->auth_key == NULL) {

		free(tnt->auth_id);
		tnt->auth_id = NULL;
		return TNT_EMEMORY;
	}

	memcpy(tnt->auth_key, key, key_size);
	return TNT_EOK;
}

void
tnt_free(struct tnt * tnt)
{
	if (tnt->connected)
		tnt_close(tnt);

	if (tnt->auth_id)
		free(tnt->auth_id);

	if (tnt->auth_key)
		free(tnt->auth_key);

	free(tnt);
}

struct {

	enum tnt_result type;
	char * desc;

} static tnt_error_list[] = 
{
	{ TNT_EFAIL,    "fail"                     },
	{ TNT_EOK,      "ok"                       },
	{ TNT_EBADVAL,  "bad function argument"    },
	{ TNT_EMEMORY,  "memory allocation failed" },
	{ TNT_ESOCKET,  "socket(2) failed"         },
	{ TNT_ESOCKOPT, "setsockopt(2) failed"     },
	{ TNT_ECONNECT, "connect(2) failed"        },
	{ TNT_EREAD,    "read(2) failed"           },
	{ TNT_EWRITE,   "write(2) failed"          },
	{ TNT_EPROTO,   "protocol sanity error"    },
	{ TNT_EAUTH,    "authorization failed"     },
	{ 0,            NULL                       }
};

char*
tnt_error(enum tnt_result res)
{
	int i;

	for (i = 0 ; tnt_error_list[i].desc ; i++)
		if (tnt_error_list[i].type == res)
			return tnt_error_list[i].desc;

	return NULL;
}

static struct sockaddr_in
get_sockaddr_in(const char *hostname, unsigned short port)
{
	struct sockaddr_in result;

	memset((void*)(&result), 0, sizeof(result));
	result.sin_family = AF_INET;
	result.sin_port   = htons(port);

	struct hostent *host = gethostbyname(hostname);

	if (host != 0)
		memcpy((void*)(&result.sin_addr),
			(void*)(host->h_addr), host->h_length);

	return result;
}

static void
tnt_auth_hash(struct tnt * tnt,
	struct auth_chap_hdr_server_1 * hs1,
	struct auth_chap_hdr_client * hc)
{
	AES_CMAC_CTX cmac;
	AES_CMAC_Init(&cmac);

	AES_CMAC_SetKey(&cmac, tnt->auth_key);
	AES_CMAC_Update(&cmac, (u_int8_t*)hs1->token, sizeof(hs1->token));

	AES_CMAC_Final((u_int8_t*)hc->hash, &cmac);
}

static enum tnt_result
tnt_auth_chap(struct tnt * tnt)
{
	/* Stage1. Recv server Token. */

	struct auth_chap_hdr_server_1 hs1;

	if (recv(tnt->port, (void*)&hs1, sizeof(struct auth_chap_hdr_server_1), 0) !=
		sizeof(struct auth_chap_hdr_server_1))
			return TNT_EFAIL;

	if (memcmp(hs1.magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE))
		return TNT_EPROTO;

	/* Stage2. Creating Hash by signing Token. */

	struct auth_chap_hdr_client hc;

	memcpy(hc.magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE);

	switch (tnt->auth_proto) {
		case TNT_PROTO_ADMIN:
			hc.proto = AUTH_CHAP_PROTO_ADMIN;
			break;
		case TNT_PROTO_FEEDER:
			hc.proto = AUTH_CHAP_PROTO_FEEDER;
			break;
		case TNT_PROTO_RW:
			hc.proto = AUTH_CHAP_PROTO_RW;
			break;
		case TNT_PROTO_RO:
			hc.proto = AUTH_CHAP_PROTO_RO;
			break;
	}

	memset(hc.id, 0, sizeof(hc.id));
	memcpy(hc.id, tnt->auth_id, tnt->auth_id_size);

	tnt_auth_hash(tnt, &hs1, &hc);

	if (send(tnt->port, (void*)&hc, sizeof(struct auth_chap_hdr_client), 0) !=
		sizeof(struct auth_chap_hdr_client))
			return TNT_EFAIL;

	/* Stage3. Check response. */

	struct auth_chap_hdr_server_2 hs2;

	if (recv(tnt->port, (void*)&hs2, sizeof(struct auth_chap_hdr_server_2), 0) !=
		sizeof(struct auth_chap_hdr_server_2))
			return TNT_EFAIL;

	if (memcmp(hs2.magic, AUTH_CHAP_MAGIC, AUTH_CHAP_MAGIC_SIZE))
		return TNT_EPROTO;

	if (hs2.resp == AUTH_CHAP_RESP_OK)
		return TNT_EOK;

	return TNT_EAUTH;
}

static enum tnt_result
tnt_auth(struct tnt * tnt)
{
	switch (tnt->auth_type) {

		case TNT_AUTH_CHAP:
			return tnt_auth_chap(tnt);
	}

	return TNT_EOK;
}

enum tnt_result
tnt_connect(struct tnt * tnt, const char *hostname, int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return TNT_ESOCKET;

	int opt = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1)
		return TNT_ESOCKOPT;

	struct sockaddr_in addr = get_sockaddr_in(hostname, port);
	if (connect(fd, (struct sockaddr*)&addr, sizeof addr))
		return TNT_ECONNECT;

	tnt->port = fd;

	if ( tnt->auth_type != TNT_AUTH_NONE ) {

		int result = tnt_auth(tnt);

		if (result != TNT_EOK) {

		  close(fd);
		  return result;
		}
	}

	tnt->connected = 1;
	return TNT_EOK;
}

void
tnt_close(struct tnt * tnt)
{
	if (tnt->connected)
		close(tnt->port);
}

int
tnt_execute_raw(struct tnt * tnt, const char * message, size_t len)
{
  if (send(tnt->port, message, len, 0) < 0)
	  return 3;

  char buf[2048];
  if (recv(tnt->port, buf, 2048, 0) < 16)
	  return 3;

  return buf[12]; // return_code: 0,1,2
}

#if 0
int
tnt_execute_raw(struct tnt_connection *conn, const char *message,
		    size_t len)
{
  if (send(conn->port, message, len, 0) < 0)
	  return 3;

  char buf[2048];
  if (recv(conn->port, buf, 2048, 0) < 16)
	  return 3;

  return buf[12]; // return_code: 0,1,2
}
#endif
