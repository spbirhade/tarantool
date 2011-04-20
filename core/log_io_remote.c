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

#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <say.h>
#include <log_io.h>
#include <pickle.h>

static u32
row_v11_len(struct tbuf *r)
{
	if (r->len < sizeof(struct row_v11))
		return 0;

	if (r->len < sizeof(struct row_v11) + row_v11(r)->len)
		return 0;

	return sizeof(struct row_v11) + row_v11(r)->len;
}

static struct tbuf *
remote_row_reader_v11()
{
	const int header_size = sizeof(struct row_v11);
	struct tbuf *m;

	for (;;) {
		if (row_v11_len(fiber->rbuf) != 0) {
			m = tbuf_split(fiber->rbuf, row_v11_len(fiber->rbuf));
			say_debug("read row bytes:%" PRIu32 " %s", m->len, tbuf_to_hex(m));
			return m;
		}

		if (fiber_bread(fiber->rbuf, header_size) <= 0) {
			say_error("unexpected eof reading row header");
			return NULL;
		}
	}
}

static struct tbuf *
remote_read_row(i64 initial_lsn)
{
	struct tbuf *row;
	bool warning_said = false;
	const int reconnect_delay = 1;
	const char *err = NULL;
	u32 version;

	for (;;) {
		if (fiber->fd < 0) {
			if (fiber_connect(fiber->data) < 0) {
				err = "can't connect to feeder";
				goto err;
			}

			if (fiber_write(&initial_lsn, sizeof(initial_lsn)) != sizeof(initial_lsn)) {
				err = "can't write version";
				goto err;
			}

			if (fiber_read(&version, sizeof(version)) != sizeof(version)) {
				err = "can't read version";
				goto err;
			}

			if (version != default_version) {
				err = "remote version mismatch";
				goto err;
			}

			say_crit("succefully connected to feeder");
			say_crit("starting remote recovery from lsn:%" PRIi64, initial_lsn);
			warning_said = false;
			err = NULL;
		}

		row = remote_row_reader_v11();
		if (row == NULL) {
			err = "can't read row";
			goto err;
		}

		return row;

	      err:
		if (err != NULL && !warning_said) {
			say_info("%s", err);
			say_info("will retry every %i second", reconnect_delay);
			warning_said = true;
		}
		fiber_close();
		fiber_sleep(reconnect_delay);
	}
}

static void
pull_from_remote(void *state)
{
	struct remote_state *h = state;
	struct tbuf *row;

	switch (setjmp(fiber->exc)) {
		case 0:
			break;

		case FIBER_EXIT:
			fiber_close();
			return;

		default:
			fiber_close();
	}

	for (;;) {
		row = remote_read_row(h->r->confirmed_lsn + 1);
		h->r->recovery_lag = ev_now() - row_v11(row)->tm;
		h->r->recovery_last_update_tstamp = ev_now();

		if (h->handler(h->r, row) < 0) {
			fiber_close();
			continue;
		}

		fiber_gc();
	}
}

int
default_remote_row_handler(struct recovery_state *r, struct tbuf *row)
{
	struct tbuf *data;
	i64 lsn = row_v11(row)->lsn;
	u16 tag;

	/* save row data since wal_row_handler may clobber it */
	data = tbuf_alloc(row->pool);
	tbuf_append(data, row_v11(row)->data, row_v11(row)->len);

	if (r->row_handler(r, row) < 0)
		panic("replication failure: can't apply row");

	tag = read_u16(data);
	(void)read_u64(data); /* drop the cookie */

	if (wal_write(r, tag, r->cookie, lsn, data) == false)
		panic("replication failure: can't write row to WAL");

	next_lsn(r, lsn);
	confirm_lsn(r, lsn);

	return 0;
}

struct fiber *
recover_follow_remote(struct recovery_state *r, char *ip_addr, int port,
		      int (*handler) (struct recovery_state *r, struct tbuf *row))
{
	char *name;
	struct fiber *f;
	struct in_addr server;
	struct sockaddr_in *addr;
	struct remote_state *h;

	say_crit("initializing remote hot standby, WAL feeder %s:%i", ip_addr, port);
	name = palloc(eter_pool, 64);
	snprintf(name, 64, "remote_hot_standby/%s:%i", ip_addr, port);

	h = palloc(eter_pool, sizeof(*h));
	h->r = r;
	h->handler = handler;

	f = fiber_create(name, -1, -1, pull_from_remote, h);
	if (f == NULL)
		return NULL;

	if (inet_aton(ip_addr, &server) < 0) {
		say_syserror("inet_aton: %s", ip_addr);
		return NULL;
	}

	addr = palloc(eter_pool, sizeof(*addr));
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	memcpy(&addr->sin_addr.s_addr, &server, sizeof(server));
	addr->sin_port = htons(port);
	f->data = addr;
	memcpy(&r->cookie, &addr, MIN(sizeof(r->cookie), sizeof(addr)));
	fiber_call(f);
	return f;
}
