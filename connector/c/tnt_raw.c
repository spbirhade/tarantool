
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt.h>
#include <tnt_raw.h>

int
tnt_raw_send(tnt_t * t, char * buf, int size)
{
	int result = send(t->fd, buf, size, 0);

	if (result == -1)
		t->error_errno = errno;

	return result;
}

int
tnt_raw_sendv(tnt_t * t, int count, ...)
{
	struct iovec * v = tnt_mem_alloc(count * sizeof(struct iovec));

	if (v == NULL)
		return -1;

	va_list args;
	int i;

	va_start(args, count);

	for (i = 0 ; i < count ; i++ ) {

		v[i].iov_len  = va_arg(args, int);
		v[i].iov_base = va_arg(args, char*);
	}

	int r = writev(t->fd, v, count);

	if (r == -1)
		t->error_errno = errno;

	tnt_mem_free(v);
	return r;
}

int
tnt_raw_recv(tnt_t * t, char * buf, int size)
{
	int result = recv(t->fd, buf, size, 0);

	if (result == -1)
		t->error_errno = errno;

	return result;
}
