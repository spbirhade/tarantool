#ifndef TNT_IO_H_
#define TNT_IO_H_

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

tnt_error_t
tnt_io_init(tnt_t * t);

void
tnt_io_free(tnt_t * t);

tnt_error_t
tnt_io_connect(tnt_t * t, char * host, int port);

void
tnt_io_close(tnt_t * t);

tnt_error_t
tnt_io_flush(tnt_t * t);

int
tnt_io_send_raw(tnt_t * t, char * buf, int size);

int
tnt_io_send_rawv(tnt_t * t, void * iovec, int count);

int
tnt_io_recv_raw(tnt_t * t, char * buf, int size);


tnt_error_t
tnt_io_send(tnt_t * t, char * buf, int size);

tnt_error_t
tnt_io_sendv(tnt_t * t, void * iovec, int count);


tnt_error_t
tnt_io_recv(tnt_t * t, char * buf, int size);

tnt_error_t
tnt_io_recv_char(tnt_t * t, char buf[1]);

tnt_error_t
tnt_io_recv_expect(tnt_t * t, char * sz);

#endif
