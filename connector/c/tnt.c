
/*
 * Copyright (C) 2011 Mail.RU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitnted provided that the following conditions
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>

#include <tnt_result.h>
#include <tnt_aes.h>
#include <tnt_cmac.h>
#include <tnt_mem.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_auth_chap.h>
#include <tnt_auth.h>

tnt_t*
tnt_init(int rbuf_size, int sbuf_size)
{
	tnt_t * t = malloc(sizeof(tnt_t));

	if (t == NULL)
		return NULL;

	memset(t, 0, sizeof(tnt_t));

	t->auth_type = TNT_AUTH_NONE;

	t->rbuf_size = rbuf_size;
	t->sbuf_size = sbuf_size;

	if (tnt_io_init(t) != TNT_EOK) {

		free(t);
		return NULL;
	}

	return t;
}

void
tnt_init_alloc(tnt_t * t,
	tnt_mallocf_t m, tnt_reallocf_t r, tnt_dupf_t d, tnt_freef_t f)
{
	(void)t;
	tnt_mem_init(m, r, d, f);
}

tnt_result_t
tnt_init_auth(tnt_t * t, tnt_auth_t auth, tnt_auth_proto_t proto,
	char * id,
	unsigned char * key, int key_size)
{
	t->auth_type = auth;

	switch (t->auth_type) {

		case TNT_AUTH_NONE:
			return TNT_EOK;

		case TNT_AUTH_CHAP:

			if ( t->auth_key_size > TNT_AES_CMAC_KEY_LENGTH )
				return TNT_EBADVAL;

			if ( (t->auth_id_size + 1) > TNT_AUTH_CHAP_ID_SIZE )
				return TNT_EBADVAL;
			break;
	}

	t->auth_proto = proto;

	t->auth_id_size = strlen(id);
	t->auth_id = malloc(t->auth_id_size);

	if (t->auth_id == NULL)
		return TNT_EMEMORY;

	memcpy(t->auth_id, id, t->auth_id_size);

	t->auth_key_size = key_size;
	t->auth_key = malloc(t->auth_key_size);

	if (t->auth_key == NULL) {

		free(t->auth_id);
		t->auth_id = NULL;
		return TNT_EMEMORY;
	}

	memcpy(t->auth_key, key, key_size);
	return TNT_EOK;
}

void
tnt_free(tnt_t * t)
{
	tnt_io_free(t);

	if (t->connected)
		tnt_close(t);

	if (t->auth_id)
		free(t->auth_id);

	if (t->auth_key)
		free(t->auth_key);

	free(t);
}

typedef struct {

	tnt_result_t type;
	char * desc;

} tnt_error_t;

/* must be in sync with enum tnt_result_t */

static
tnt_error_t tnt_error_list[] = 
{
	{ TNT_EFAIL,     "fail"                     },
	{ TNT_EOK,       "ok"                       },
	{ TNT_EBADVAL,   "bad function argument"    },
	{ TNT_EMEMORY,   "memory allocation failed" },
	{ TNT_EBIG,      "buffer is too big"        },
	{ TNT_ESIZE,     "bad buffer size"          },
	{ TNT_ESOCKET,   "socket(2) failed"         },
	{ TNT_ESOCKOPT,  "setsockopt(2) failed"     },
	{ TNT_ECONNECT,  "connect(2) failed"        },
	{ TNT_EREAD,     "recv(2) failed"           },
	{ TNT_EWRITE,    "write(2) failed"          },
	{ TNT_EPROTO,    "protocol sanity error"    },
	{ TNT_EAUTH,     "authorization failed"     },
	{ TNT_ENOOP,     "no update ops specified"  },
	{ TNT_ENOTU,     "no tuples specified"      },
	{ TNT_EERROR,    "error"                    },
	{ TNT_EAGAIN,    "resend needed"            },
	{ TNT_LAST,       NULL                      }
};

char*
tnt_error(tnt_result_t res)
{
	if ( (int)res > TNT_LAST )
		return NULL;

	return tnt_error_list[(int)res].desc;
}

static struct sockaddr_in
tnt_connect_resolve(const char *hostname, unsigned short port)
{
	struct sockaddr_in result;

	memset((void*)(&result), 0, sizeof(result));

	result.sin_family = AF_INET;
	result.sin_port   = htons(port);

	struct hostent * host = gethostbyname(hostname);

	if (host)
		memcpy((void*)(&result.sin_addr),
			(void*)(host->h_addr), host->h_length);

	return result;
}

tnt_result_t
tnt_connect(tnt_t * t, char * hostname, int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		return TNT_ESOCKET;

	int opt = 1;

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1)
		return TNT_ESOCKOPT;

	opt = 3493888;

	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) == -1)
		return TNT_ESOCKOPT;

	opt = 3493888;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) == -1)
		return TNT_ESOCKOPT;

	struct sockaddr_in addr = tnt_connect_resolve(hostname, port);

	if (connect(fd, (struct sockaddr*)&addr, sizeof addr))
		return TNT_ECONNECT;

	t->fd = fd;

	if (t->auth_type != TNT_AUTH_NONE) {

		int result = tnt_auth(t);

		if (result != TNT_EOK) {

		  close(fd);
		  return result;
		}
	}

	t->connected = 1;
	return TNT_EOK;
}

tnt_result_t
tnt_flush(tnt_t * t)
{
	return tnt_io_flush(t);
}

void
tnt_close(tnt_t * t)
{
	if (t->connected)
		close(t->fd);
}
