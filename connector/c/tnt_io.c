
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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <tnt_result.h>
#include <tnt_mem.h>
#include <tnt.h>
#include <tnt_io.h>

tnt_result_t
tnt_io_init(tnt_t * t)
{
	t->fd = -1;

	if (t->rbuf_size) {

		t->rbuf = tnt_mem_alloc(t->rbuf_size);

		if (t->rbuf == NULL)
			return TNT_EMEMORY;
	}

	if (t->sbuf_size) {

		t->sbuf = tnt_mem_alloc(t->sbuf_size);

		if (t->sbuf == NULL) {

			if (t->rbuf) {

				free(t->rbuf);
				t->rbuf = NULL;
			}

			return TNT_EMEMORY;
		}
	}

	return TNT_EOK;
}

void
tnt_io_free(tnt_t * t)
{
	tnt_io_close(t);

	if (t->rbuf)
		tnt_mem_free(t->rbuf);

	if (t->sbuf)
		tnt_mem_free(t->sbuf);
}

static tnt_result_t
tnt_io_resolve(struct sockaddr_in * addr,
	const char * hostname, unsigned short port)
{
	memset(addr, 0, sizeof(struct sockaddr_in));

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);

	struct hostent * host = gethostbyname(hostname);

	if (host)
		memcpy(&addr->sin_addr,
			(void*)(host->h_addr), host->h_length);
	else
		return TNT_ERESOLVE;

	return TNT_EOK;
}

static tnt_result_t
tnt_io_nonblock(tnt_t * t, int set)
{
	int flags = fcntl(t->fd, F_GETFL);

	if (flags == -1)
		return TNT_ENONBLOCK;

	if (set)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if (fcntl(t->fd, F_SETFL, flags) == -1)
		return TNT_ENONBLOCK;

	return TNT_EOK;
}

static tnt_result_t
tnt_io_connect_do(tnt_t * t, char * host, int port)
{
	struct sockaddr_in addr;
	
	tnt_result_t result = tnt_io_resolve(&addr, host, port);

	if (result != TNT_EOK)
		return result;

	result = tnt_io_nonblock(t, 1);

	if (result != TNT_EOK)
		return result;

	if (connect(t->fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {

		if (errno == EINPROGRESS) {

			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(t->fd, &fds);

			struct timeval tmout;
			tmout.tv_sec  = t->opt_tmout;
			tmout.tv_usec = 0;

			if (select(t->fd + 1, NULL, &fds, NULL, &tmout) == -1)
				return TNT_ECONNECT;

			if (!FD_ISSET(t->fd, &fds))
				return TNT_ETMOUT;

			int opt = 0;
			socklen_t len = sizeof(opt);

			if ((getsockopt(t->fd, SOL_SOCKET, SO_ERROR, &opt, &len) == -1) || opt)
				return TNT_ECONNECT;

		} else
			return TNT_ECONNECT;
	}

	result = tnt_io_nonblock(t, 0);

	if (result != TNT_EOK)
		return result;

	return TNT_EOK;
}

static tnt_result_t
tnt_io_setopts(tnt_t * t)
{
	int opt = 1;

	if (setsockopt(t->fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) {

		tnt_io_close(t);
		return TNT_ESOCKOPT;
	}

	if (t->sbuf_size)
		opt = t->sbuf_size * 2;
	else
		opt = 3493888;

	if (setsockopt(t->fd, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) == -1) {

		tnt_io_close(t);
		return TNT_ESOCKOPT;
	}

	if (t->rbuf_size)
		opt = t->rbuf_size * 2;
	else
		opt = 3493888;

	if (setsockopt(t->fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) == -1) {

		tnt_io_close(t);
		return TNT_ESOCKOPT;
	}

	if (t->opt_tmout_snd) {

		struct timeval tmout;

		tmout.tv_sec  = t->opt_tmout_snd;
		tmout.tv_usec = 0;

		if (setsockopt(t->fd, SOL_SOCKET, SO_SNDTIMEO, &tmout, sizeof(tmout)) == -1) {

			tnt_io_close(t);
			return TNT_ESOCKOPT;
		}
	}

	if (t->opt_tmout_rcv) {

		struct timeval tmout;

		tmout.tv_sec  = t->opt_tmout_rcv;
		tmout.tv_usec = 0;

		if (setsockopt(t->fd, SOL_SOCKET, SO_RCVTIMEO, &tmout, sizeof(tmout)) == -1) {

			tnt_io_close(t);
			return TNT_ESOCKOPT;
		}
	}

	return TNT_EOK;
}

tnt_result_t
tnt_io_connect(tnt_t * t, char * host, int port)
{
	t->fd = socket(AF_INET, SOCK_STREAM, 0);

	if (t->fd < 0)
		return TNT_ESOCKET;

	tnt_result_t result = tnt_io_setopts(t);

	if (result != TNT_EOK)
		return result;

	result = tnt_io_connect_do(t, host, port);

	if (result != TNT_EOK) {

		tnt_io_close(t);
		return result;
	}

	return TNT_EOK;
}

void
tnt_io_close(tnt_t * t)
{
	if (t->fd >= 0) {

		close(t->fd);
		t->fd = -1;
	}

	t->connected = 0;
}

tnt_result_t
tnt_io_flush(tnt_t * t)
{
	if (t->sbuf_off) {

		tnt_result_t r = tnt_io_send(t, t->sbuf, t->sbuf_off);

		if (r != TNT_EOK)
			return r;

		t->sbuf_off = 0;
	}

	return TNT_EOK;
}

tnt_result_t
tnt_io_send(tnt_t * t, char * buf, int size)
{
	int r, off = 0;

	do {
		r = send(t->fd, buf + off, size - off, 0);

		if (r == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			else
				return TNT_EWRITE;
		}

		off += r;
	} while (off != size);

	return TNT_EOK;
}

static tnt_result_t
tnt_io_sendv_asis(tnt_t * t, void * iovec, int count)
{
	if (writev(t->fd, iovec, count) == -1)
		return TNT_EWRITE;

	return TNT_EOK;
}

inline static void
tnt_io_sendv_put(tnt_t * t, void * iovec, int count)
{
	int i;

	for (i = 0 ; i < count ; i++) {

		memcpy(t->sbuf + t->sbuf_off,  
			((struct iovec*)iovec)[i].iov_base,
			((struct iovec*)iovec)[i].iov_len);

		t->sbuf_off += ((struct iovec*)iovec)[i].iov_len;
	}
}

tnt_result_t
tnt_io_sendv(tnt_t * t, void * iovec, int count)
{
	if (t->sbuf == NULL)
		return tnt_io_sendv_asis(t, iovec, count);

	int i, size = 0;

	for (i = 0 ; i < count ; i++)
		size += ((struct iovec*)iovec)[i].iov_len;

	if (size > t->sbuf_size)
		return TNT_EBIG;

	if ((t->sbuf_off + size) <= t->sbuf_size) {

		tnt_io_sendv_put(t, iovec, count);
		return TNT_EOK;
	}

	tnt_result_t r = tnt_io_send(t, t->sbuf, t->sbuf_off);

	if (r != TNT_EOK)
		return r;

	t->sbuf_off = 0;
	tnt_io_sendv_put(t, iovec, count);

	return TNT_EOK;
}

static tnt_result_t
tnt_io_recv_asis(tnt_t * t, char * buf, int size, int off)
{
	do {
		int r = recv(t->fd, buf + off, size - off, 0);

		if (r == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			else
				return TNT_EREAD;
		}

		off += r;
	} while (off != size);

	return TNT_EOK;
}

tnt_result_t
tnt_io_recv(tnt_t * t, char * buf, int size)
{
	if (t->rbuf == NULL)
		return tnt_io_recv_asis(t, buf, size, 0);

	int lv, rv, off = 0, left = size;

	while (1) {

		if ((t->rbuf_off + left) <= t->rbuf_top) {

			memcpy(buf + off, t->rbuf + t->rbuf_off, left);
			t->rbuf_off += left;

			return TNT_EOK;
		}

		lv = t->rbuf_top - t->rbuf_off;
		rv = left - lv;

		if (lv) {

			memcpy(buf + off, t->rbuf + t->rbuf_off, lv);
			off += lv;
		}

		while (1) {

			t->rbuf_top = recv(t->fd, t->rbuf, t->rbuf_size, 0);

			if (t->rbuf_top == -1) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				else
					return TNT_EREAD;
			}

			break;
		} 

		t->rbuf_off = 0;

		if (rv <= t->rbuf_top) {

			memcpy(buf + off, t->rbuf, rv);
			t->rbuf_off = rv;

			return TNT_EOK;
		}

		left -= lv;
	}

	return TNT_EOK;
}

tnt_result_t
tnt_io_recv_continue(tnt_t * t, char * buf, int size, int off)
{
	return tnt_io_recv(t, buf + off, size - off);
}
