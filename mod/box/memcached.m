
#line 1 "mod/box/memcached.rl"
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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

#include <errcode.h>
#include <salloc.h>
#include <palloc.h>
#include <fiber.h>
#include <util.h>
#include <pickle.h>
#include "say.h"

#include <tarantool.h>
#include <cfg/tarantool_box_cfg.h>
#include <mod/box/box.h>
#include <stat.h>


#define STAT(_)					\
        _(MEMC_GET, 1)				\
        _(MEMC_GET_MISS, 2)			\
	_(MEMC_GET_HIT, 3)			\
	_(MEMC_EXPIRED_KEYS, 4)

ENUM(memcached_stat, STAT);
STRS(memcached_stat, STAT);
int stat_base;

struct index *memcached_index;

/* memcached tuple format:
   <key, meta, data> */

struct meta {
	u32 exptime;
	u32 flags;
	u64 cas;
} __packed__;


#line 72 "mod/box/memcached.m"
static const int memcached_start = 1;
static const int memcached_first_final = 197;
static const int memcached_error = 0;

static const int memcached_en_main = 1;


#line 71 "mod/box/memcached.rl"



static u64
natoq(const u8 *start, const u8 *end)
{
	u64 num = 0;
	while (start < end)
		num = num * 10 + (*start++ - '0');
	return num;
}

static int
store(struct box_txn *txn, void *key, u32 exptime, u32 flags, u32 bytes, u8 *data)
{
	u32 box_flags = BOX_QUIET, cardinality = 4;
	static u64 cas = 42;
	struct meta m;

	struct tbuf *req = tbuf_alloc(fiber->pool);

	tbuf_append(req, &cfg.memcached_namespace, sizeof(u32));
	tbuf_append(req, &box_flags, sizeof(box_flags));
	tbuf_append(req, &cardinality, sizeof(cardinality));

	tbuf_append_field(req, key);

	m.exptime = exptime;
	m.flags = flags;
	m.cas = cas++;
	write_varint32(req, sizeof(m));
	tbuf_append(req, &m, sizeof(m));

	char b[43];
	sprintf(b, " %"PRIu32" %"PRIu32"\r\n", flags, bytes);
	write_varint32(req, strlen(b));
	tbuf_append(req, b, strlen(b));

	write_varint32(req, bytes);
	tbuf_append(req, data, bytes);

	int key_len = load_varint32(&key);
	say_debug("memcached/store key:(%i)'%.*s' exptime:%"PRIu32" flags:%"PRIu32" cas:%"PRIu64,
		  key_len, key_len, (u8 *)key, exptime, flags, cas);
	return box_process(txn, INSERT, req); /* FIXME: handle RW/RO */
}

static int
delete(struct box_txn *txn, void *key)
{
	u32 key_len = 1;
	struct tbuf *req = tbuf_alloc(fiber->pool);

	tbuf_append(req, &cfg.memcached_namespace, sizeof(u32));
	tbuf_append(req, &key_len, sizeof(key_len));
	tbuf_append_field(req, key);

	return box_process(txn, DELETE, req);
}

static struct box_tuple *
find(void *key)
{
	return memcached_index->find(memcached_index, key);
}

static struct meta *
meta(struct box_tuple *tuple)
{
	void *field = tuple_field(tuple, 1);
	return field + 1;
}

static bool
expired(struct box_tuple *tuple)
{
	struct meta *m = meta(tuple);
 	return m->exptime == 0 ? 0 : m->exptime < ev_now();
}

static bool
is_numeric(void *field, u32 value_len)
{
	for (int i = 0; i < value_len; i++)
		if (*((u8 *)field + i) < '0' || '9' < *((u8 *)field + i))
			return false;
	return true;
}

static struct stats {
	u64 total_items;
	u32 curr_connections;
	u32 total_connections;
	u64 cmd_get;
	u64 cmd_set;
	u64 get_hits;
	u64 get_misses;
	u64 evictions;
	u64 bytes_read;
	u64 bytes_written;
} stats;

static void
print_stats()
{
	u64 bytes_used, items;
	struct tbuf *out = tbuf_alloc(fiber->pool);
	slab_stat2(&bytes_used, &items);

	tbuf_printf(out, "STAT pid %"PRIu32"\r\n", (u32)getpid());
	tbuf_printf(out, "STAT uptime %"PRIu32"\r\n", (u32)tarantool_uptime());
	tbuf_printf(out, "STAT time %"PRIu32"\r\n", (u32)ev_now());
	tbuf_printf(out, "STAT version 1.2.5 (tarantool/box)\r\n");
	tbuf_printf(out, "STAT pointer_size %"PRI_SZ"\r\n", sizeof(void *)*8);
	tbuf_printf(out, "STAT curr_items %"PRIu64"\r\n", items);
	tbuf_printf(out, "STAT total_items %"PRIu64"\r\n", stats.total_items);
	tbuf_printf(out, "STAT bytes %"PRIu64"\r\n", bytes_used);
	tbuf_printf(out, "STAT curr_connections %"PRIu32"\r\n", stats.curr_connections);
	tbuf_printf(out, "STAT total_connections %"PRIu32"\r\n", stats.total_connections);
	tbuf_printf(out, "STAT connection_structures %"PRIu32"\r\n", stats.curr_connections); /* lie a bit */
	tbuf_printf(out, "STAT cmd_get %"PRIu64"\r\n", stats.cmd_get);
	tbuf_printf(out, "STAT cmd_set %"PRIu64"\r\n", stats.cmd_set);
	tbuf_printf(out, "STAT get_hits %"PRIu64"\r\n", stats.get_hits);
	tbuf_printf(out, "STAT get_misses %"PRIu64"\r\n", stats.get_misses);
	tbuf_printf(out, "STAT evictions %"PRIu64"\r\n", stats.evictions);
	tbuf_printf(out, "STAT bytes_read %"PRIu64"\r\n", stats.bytes_read);
	tbuf_printf(out, "STAT bytes_written %"PRIu64"\r\n", stats.bytes_written);
	tbuf_printf(out, "STAT limit_maxbytes %"PRIu64"\r\n", (u64)(cfg.slab_alloc_arena * (1 << 30)));
	tbuf_printf(out, "STAT threads 1\r\n");
	tbuf_printf(out, "END\r\n");
	add_iov(out->data, out->len);
}

static void
flush_all(void *data)
{
	uintptr_t delay = (uintptr_t)data;
	fiber_sleep(delay - ev_now());
	khash_t(lstr_ptr_map) *map = memcached_index->idx.str_hash;
	for (khiter_t i = kh_begin(map); i != kh_end(map); i++) {
		if (kh_exist(map, i)) {
			struct box_tuple *tuple = kh_value(map, i);
			meta(tuple)->exptime = 1;
		}
	}
}


static int __attribute__((noinline))
memcached_dispatch(struct box_txn *txn)
{
	int cs;
	u8 *p, *pe;
	u8 *fstart;
	struct tbuf *keys = tbuf_alloc(fiber->pool);
	void *key;
	bool append, show_cas;
	int incr_sign;
	u64 cas, incr;
	u32 flags, exptime, bytes;
	bool noreply = false;
	u8 *data = NULL;
	bool done = false;
	int r;
	size_t saved_iov_cnt = fiber->iov_cnt;
	uintptr_t flush_delay = 0;
	size_t keys_count = 0;

	p = fiber->rbuf->data;
	pe = fiber->rbuf->data + fiber->rbuf->len;

	say_debug("memcached_dispatch '%.*s'", MIN((int)(pe - p), 40) , p);

#define STORE ({									\
	stats.cmd_set++;								\
	if (bytes > (1<<20)) {								\
		add_iov("SERVER_ERROR object too large for cache\r\n", 41);		\
	} else {									\
		u32 ret_code;								\
		if ((ret_code = store(txn, key, exptime, flags, bytes, data)) == 0) {	\
			stats.total_items++;						\
			add_iov("STORED\r\n", 8);					\
		} else {								\
			add_iov("SERVER_ERROR ", 13);					\
			add_iov(tnt_errcode_desc(ret_code),			\
				strlen(tnt_errcode_desc(ret_code)));		\
			add_iov("\r\n", 2);						\
		}									\
	}										\
})

	
#line 273 "mod/box/memcached.m"
	{
	cs = memcached_start;
	}

#line 278 "mod/box/memcached.m"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
case 1:
	switch( (*p) ) {
		case 97: goto st2;
		case 99: goto st44;
		case 100: goto st67;
		case 102: goto st103;
		case 103: goto st124;
		case 105: goto st132;
		case 112: goto st136;
		case 113: goto st143;
		case 114: goto st148;
		case 115: goto st172;
	}
	goto st0;
st0:
cs = 0;
	goto _out;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 100: goto st3;
		case 112: goto st22;
	}
	goto st0;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	if ( (*p) == 100 )
		goto st4;
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) == 32 )
		goto st5;
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st5;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr15;
tr15:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st6;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
#line 353 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st7;
	goto st0;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	if ( (*p) == 32 )
		goto st7;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr17;
	goto st0;
tr17:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
#line 374 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr18;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st8;
	goto st0;
tr18:
#line 512 "mod/box/memcached.rl"
	{flags = natoq(fstart, p);}
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 388 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st9;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr21;
	goto st0;
tr21:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st10;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
#line 402 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr22;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st10;
	goto st0;
tr22:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st11;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
#line 420 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st11;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr25;
	goto st0;
tr25:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st12;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
#line 434 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr26;
		case 13: goto tr27;
		case 32: goto tr28;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st12;
	goto st0;
tr26:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 268 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple != NULL && !expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr30:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 268 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple != NULL && !expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr39:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 268 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple != NULL && !expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr58:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 297 "mod/box/memcached.rl"
	{
			struct tbuf *b;
			void *value;
			u32 value_len;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST) {
				add_iov("NOT_STORED\r\n", 12);
			} else {
				value = tuple_field(tuple, 3);
				value_len = load_varint32(&value);
				b = tbuf_alloc(fiber->pool);
				if (append) {
					tbuf_append(b, value, value_len);
					tbuf_append(b, data, bytes);
				} else {
					tbuf_append(b, data, bytes);
					tbuf_append(b, value, value_len);
				}

				bytes += value_len;
				data = b->data;
				STORE;
			}
		}
	goto st197;
tr62:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 297 "mod/box/memcached.rl"
	{
			struct tbuf *b;
			void *value;
			u32 value_len;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST) {
				add_iov("NOT_STORED\r\n", 12);
			} else {
				value = tuple_field(tuple, 3);
				value_len = load_varint32(&value);
				b = tbuf_alloc(fiber->pool);
				if (append) {
					tbuf_append(b, value, value_len);
					tbuf_append(b, data, bytes);
				} else {
					tbuf_append(b, data, bytes);
					tbuf_append(b, value, value_len);
				}

				bytes += value_len;
				data = b->data;
				STORE;
			}
		}
	goto st197;
tr71:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 297 "mod/box/memcached.rl"
	{
			struct tbuf *b;
			void *value;
			u32 value_len;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST) {
				add_iov("NOT_STORED\r\n", 12);
			} else {
				value = tuple_field(tuple, 3);
				value_len = load_varint32(&value);
				b = tbuf_alloc(fiber->pool);
				if (append) {
					tbuf_append(b, value, value_len);
					tbuf_append(b, data, bytes);
				} else {
					tbuf_append(b, data, bytes);
					tbuf_append(b, value, value_len);
				}

				bytes += value_len;
				data = b->data;
				STORE;
			}
		}
	goto st197;
tr91:
#line 514 "mod/box/memcached.rl"
	{cas = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 286 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_FOUND\r\n", 11);
			else if (meta(tuple)->cas != cas)
				add_iov("EXISTS\r\n", 8);
			else
				STORE;
		}
	goto st197;
tr95:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 286 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_FOUND\r\n", 11);
			else if (meta(tuple)->cas != cas)
				add_iov("EXISTS\r\n", 8);
			else
				STORE;
		}
	goto st197;
tr105:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 286 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_FOUND\r\n", 11);
			else if (meta(tuple)->cas != cas)
				add_iov("EXISTS\r\n", 8);
			else
				STORE;
		}
	goto st197;
tr118:
#line 515 "mod/box/memcached.rl"
	{incr = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 324 "mod/box/memcached.rl"
	{
			struct meta *m;
			struct tbuf *b;
			void *field;
			u32 value_len;
			u64 value;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				m = meta(tuple);
				field = tuple_field(tuple, 3);
				value_len = load_varint32(&field);

				if (is_numeric(field, value_len)) {
					value = natoq(field, field + value_len);

					if (incr_sign > 0) {
						value += incr;
					} else {
						if (incr > value)
							value = 0;
						else
							value -= incr;
					}

					exptime = m->exptime;
					flags = m->flags;

					b = tbuf_alloc(fiber->pool);
					tbuf_printf(b, "%"PRIu64, value);
					data = b->data;
					bytes = b->len;

					stats.cmd_set++;
					if (store(txn, key, exptime, flags, bytes, data) == 0) {
						stats.total_items++;
						add_iov(b->data, b->len);
						add_iov("\r\n", 2);
					} else {
						add_iov("SERVER_ERROR\r\n", 14);
					}
				} else {
					add_iov("CLIENT_ERROR cannot increment or decrement non-numeric value\r\n", 62);
				}
			}

		}
	goto st197;
tr122:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 324 "mod/box/memcached.rl"
	{
			struct meta *m;
			struct tbuf *b;
			void *field;
			u32 value_len;
			u64 value;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				m = meta(tuple);
				field = tuple_field(tuple, 3);
				value_len = load_varint32(&field);

				if (is_numeric(field, value_len)) {
					value = natoq(field, field + value_len);

					if (incr_sign > 0) {
						value += incr;
					} else {
						if (incr > value)
							value = 0;
						else
							value -= incr;
					}

					exptime = m->exptime;
					flags = m->flags;

					b = tbuf_alloc(fiber->pool);
					tbuf_printf(b, "%"PRIu64, value);
					data = b->data;
					bytes = b->len;

					stats.cmd_set++;
					if (store(txn, key, exptime, flags, bytes, data) == 0) {
						stats.total_items++;
						add_iov(b->data, b->len);
						add_iov("\r\n", 2);
					} else {
						add_iov("SERVER_ERROR\r\n", 14);
					}
				} else {
					add_iov("CLIENT_ERROR cannot increment or decrement non-numeric value\r\n", 62);
				}
			}

		}
	goto st197;
tr132:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 324 "mod/box/memcached.rl"
	{
			struct meta *m;
			struct tbuf *b;
			void *field;
			u32 value_len;
			u64 value;

			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				m = meta(tuple);
				field = tuple_field(tuple, 3);
				value_len = load_varint32(&field);

				if (is_numeric(field, value_len)) {
					value = natoq(field, field + value_len);

					if (incr_sign > 0) {
						value += incr;
					} else {
						if (incr > value)
							value = 0;
						else
							value -= incr;
					}

					exptime = m->exptime;
					flags = m->flags;

					b = tbuf_alloc(fiber->pool);
					tbuf_printf(b, "%"PRIu64, value);
					data = b->data;
					bytes = b->len;

					stats.cmd_set++;
					if (store(txn, key, exptime, flags, bytes, data) == 0) {
						stats.total_items++;
						add_iov(b->data, b->len);
						add_iov("\r\n", 2);
					} else {
						add_iov("SERVER_ERROR\r\n", 14);
					}
				} else {
					add_iov("CLIENT_ERROR cannot increment or decrement non-numeric value\r\n", 62);
				}
			}

		}
	goto st197;
tr141:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 375 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				u32 ret_code;
				if ((ret_code = delete(txn, key)) == 0)
					add_iov("DELETED\r\n", 9);
				else {
					add_iov("SERVER_ERROR ", 13);
					add_iov(tnt_errcode_desc(ret_code),
						strlen(tnt_errcode_desc(ret_code)));
					add_iov("\r\n", 2);
				}
			}
		}
	goto st197;
tr146:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 375 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				u32 ret_code;
				if ((ret_code = delete(txn, key)) == 0)
					add_iov("DELETED\r\n", 9);
				else {
					add_iov("SERVER_ERROR ", 13);
					add_iov(tnt_errcode_desc(ret_code),
						strlen(tnt_errcode_desc(ret_code)));
					add_iov("\r\n", 2);
				}
			}
		}
	goto st197;
tr157:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 375 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || tuple->flags & GHOST || expired(tuple)) {
				add_iov("NOT_FOUND\r\n", 11);
			} else {
				u32 ret_code;
				if ((ret_code = delete(txn, key)) == 0)
					add_iov("DELETED\r\n", 9);
				else {
					add_iov("SERVER_ERROR ", 13);
					add_iov(tnt_errcode_desc(ret_code),
						strlen(tnt_errcode_desc(ret_code)));
					add_iov("\r\n", 2);
				}
			}
		}
	goto st197;
tr169:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 470 "mod/box/memcached.rl"
	{
			if (flush_delay > 0) {
				struct fiber *f = fiber_create("flush_all", -1, -1, flush_all, (void *)flush_delay);
				if (f)
					fiber_call(f);
			} else
				flush_all((void *)0);
			add_iov("OK\r\n", 4);
		}
	goto st197;
tr174:
#line 516 "mod/box/memcached.rl"
	{flush_delay = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 470 "mod/box/memcached.rl"
	{
			if (flush_delay > 0) {
				struct fiber *f = fiber_create("flush_all", -1, -1, flush_all, (void *)flush_delay);
				if (f)
					fiber_call(f);
			} else
				flush_all((void *)0);
			add_iov("OK\r\n", 4);
		}
	goto st197;
tr185:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 470 "mod/box/memcached.rl"
	{
			if (flush_delay > 0) {
				struct fiber *f = fiber_create("flush_all", -1, -1, flush_all, (void *)flush_delay);
				if (f)
					fiber_call(f);
			} else
				flush_all((void *)0);
			add_iov("OK\r\n", 4);
		}
	goto st197;
tr195:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 393 "mod/box/memcached.rl"
	{
			txn->op = SELECT;
			fiber_register_cleanup((void *)txn_cleanup, txn);
			stat_collect(stat_base, MEMC_GET, 1);
			stats.cmd_get++;
			say_debug("ensuring space for %"PRI_SZ" keys", keys_count);
			iov_ensure(keys_count * 5 + 1);
			while (keys_count-- > 0) {
				struct box_tuple *tuple;
				struct meta *m;
				void *field;
				void *value;
				void *suffix;
				u32 key_len;
				u32 value_len;
				u32 suffix_len;
				u32 _l;

				key = read_field(keys);
				tuple = find(key);
				key_len = load_varint32(&key);

				if (tuple == NULL || tuple->flags & GHOST) {
					stat_collect(stat_base, MEMC_GET_MISS, 1);
					stats.get_misses++;
					continue;
				}

				field = tuple->data;

				/* skip key */
				_l = load_varint32(&field);
				field += _l;

				/* metainfo */
				_l = load_varint32(&field);
				m = field;
				field += _l;

				/* suffix */
				suffix_len = load_varint32(&field);
				suffix = field;
				field += suffix_len;

				/* value */
				value_len = load_varint32(&field);
				value = field;

				if (m->exptime > 0 && m->exptime < ev_now()) {
					stats.get_misses++;
					stat_collect(stat_base, MEMC_GET_MISS, 1);
					continue;
				} else {
					stats.get_hits++;
					stat_collect(stat_base, MEMC_GET_HIT, 1);
				}

				tuple_txn_ref(txn, tuple);

				if (show_cas) {
					struct tbuf *b = tbuf_alloc(fiber->pool);
					tbuf_printf(b, "VALUE %.*s %"PRIu32" %"PRIu32" %"PRIu64"\r\n", key_len, (u8 *)key, m->flags, value_len, m->cas);
					add_iov_unsafe(b->data, b->len);
					stats.bytes_written += b->len;
				} else {
					add_iov_unsafe("VALUE ", 6);
					add_iov_unsafe(key, key_len);
					add_iov_unsafe(suffix, suffix_len);
				}
				add_iov_unsafe(value, value_len);
				add_iov_unsafe("\r\n", 2);
				stats.bytes_written += value_len + 2;
			}
			add_iov_unsafe("END\r\n", 5);
			stats.bytes_written += 5;
		}
	goto st197;
tr213:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 484 "mod/box/memcached.rl"
	{
			return 0;
		}
	goto st197;
tr233:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 277 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr237:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 277 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr246:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 277 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			struct box_tuple *tuple = find(key);
			if (tuple == NULL || expired(tuple))
				add_iov("NOT_STORED\r\n", 12);
			else
				STORE;
		}
	goto st197;
tr263:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 263 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			STORE;
		}
	goto st197;
tr267:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 263 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			STORE;
		}
	goto st197;
tr276:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 518 "mod/box/memcached.rl"
	{
			size_t parsed = p - (u8 *)fiber->rbuf->data;
			while (fiber->rbuf->len - parsed < bytes + 2) {
				if ((r = fiber_bread(fiber->rbuf, bytes + 2 - (pe - p))) <= 0) {
					say_debug("read returned %i, closing connection", r);
					return 0;
				}
			}

			p = fiber->rbuf->data + parsed;
			pe = fiber->rbuf->data + fiber->rbuf->len;

			data = p;

			if (strncmp((char *)(p + bytes), "\r\n", 2) == 0) {
				p += bytes + 2;
			} else {
				goto exit;
			}
		}
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 263 "mod/box/memcached.rl"
	{
			key = read_field(keys);
			STORE;
		}
	goto st197;
tr281:
#line 545 "mod/box/memcached.rl"
	{ p++; }
#line 539 "mod/box/memcached.rl"
	{
			done = true;
			stats.bytes_read += p - (u8 *)fiber->rbuf->data;
			tbuf_peek(fiber->rbuf, p - (u8 *)fiber->rbuf->data);
		}
#line 480 "mod/box/memcached.rl"
	{
			print_stats();
		}
	goto st197;
st197:
	if ( ++p == pe )
		goto _test_eof197;
case 197:
#line 1573 "mod/box/memcached.m"
	goto st0;
tr27:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st13;
tr40:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st13;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
#line 1587 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr30;
	goto st0;
tr28:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st14;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
#line 1599 "mod/box/memcached.m"
	switch( (*p) ) {
		case 32: goto st14;
		case 110: goto st15;
	}
	goto st0;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	if ( (*p) == 111 )
		goto st16;
	goto st0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) == 114 )
		goto st17;
	goto st0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( (*p) == 101 )
		goto st18;
	goto st0;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 112 )
		goto st19;
	goto st0;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	if ( (*p) == 108 )
		goto st20;
	goto st0;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	if ( (*p) == 121 )
		goto st21;
	goto st0;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	switch( (*p) ) {
		case 10: goto tr39;
		case 13: goto tr40;
	}
	goto st0;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( (*p) == 112 )
		goto st23;
	goto st0;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( (*p) == 101 )
		goto st24;
	goto st0;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( (*p) == 110 )
		goto st25;
	goto st0;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 100 )
		goto st26;
	goto st0;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	if ( (*p) == 32 )
		goto tr45;
	goto st0;
tr45:
#line 553 "mod/box/memcached.rl"
	{append = true; }
	goto st27;
tr209:
#line 554 "mod/box/memcached.rl"
	{append = false;}
	goto st27;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
#line 1703 "mod/box/memcached.m"
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st27;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr46;
tr46:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st28;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
#line 1729 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st29;
	goto st0;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( (*p) == 32 )
		goto st29;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr49;
	goto st0;
tr49:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st30;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
#line 1750 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr50;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st30;
	goto st0;
tr50:
#line 512 "mod/box/memcached.rl"
	{flags = natoq(fstart, p);}
	goto st31;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
#line 1764 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st31;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr53;
	goto st0;
tr53:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st32;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
#line 1778 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr54;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st32;
	goto st0;
tr54:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st33;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
#line 1796 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st33;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr57;
	goto st0;
tr57:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st34;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
#line 1810 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr58;
		case 13: goto tr59;
		case 32: goto tr60;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st34;
	goto st0;
tr59:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st35;
tr72:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st35;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
#line 1831 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr62;
	goto st0;
tr60:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st36;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
#line 1843 "mod/box/memcached.m"
	switch( (*p) ) {
		case 32: goto st36;
		case 110: goto st37;
	}
	goto st0;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	if ( (*p) == 111 )
		goto st38;
	goto st0;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	if ( (*p) == 114 )
		goto st39;
	goto st0;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
	if ( (*p) == 101 )
		goto st40;
	goto st0;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
	if ( (*p) == 112 )
		goto st41;
	goto st0;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
	if ( (*p) == 108 )
		goto st42;
	goto st0;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
	if ( (*p) == 121 )
		goto st43;
	goto st0;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
	switch( (*p) ) {
		case 10: goto tr71;
		case 13: goto tr72;
	}
	goto st0;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
	if ( (*p) == 97 )
		goto st45;
	goto st0;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	if ( (*p) == 115 )
		goto st46;
	goto st0;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	if ( (*p) == 32 )
		goto st47;
	goto st0;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st47;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr76;
tr76:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st48;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
#line 1950 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st49;
	goto st0;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
	if ( (*p) == 32 )
		goto st49;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr78;
	goto st0;
tr78:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st50;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
#line 1971 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr79;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st50;
	goto st0;
tr79:
#line 512 "mod/box/memcached.rl"
	{flags = natoq(fstart, p);}
	goto st51;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
#line 1985 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st51;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr82;
	goto st0;
tr82:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st52;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
#line 1999 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr83;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st52;
	goto st0;
tr83:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st53;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
#line 2017 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st53;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr86;
	goto st0;
tr86:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st54;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
#line 2031 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr87;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st54;
	goto st0;
tr87:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st55;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
#line 2045 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st55;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr90;
	goto st0;
tr90:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st56;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
#line 2059 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr91;
		case 13: goto tr92;
		case 32: goto tr93;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st56;
	goto st0;
tr106:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st57;
tr92:
#line 514 "mod/box/memcached.rl"
	{cas = natoq(fstart, p);}
	goto st57;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
#line 2080 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr95;
	goto st0;
tr93:
#line 514 "mod/box/memcached.rl"
	{cas = natoq(fstart, p);}
	goto st58;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
#line 2092 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr95;
		case 13: goto st57;
		case 32: goto st58;
		case 110: goto st59;
	}
	goto st0;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	if ( (*p) == 111 )
		goto st60;
	goto st0;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	if ( (*p) == 114 )
		goto st61;
	goto st0;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	if ( (*p) == 101 )
		goto st62;
	goto st0;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	if ( (*p) == 112 )
		goto st63;
	goto st0;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	if ( (*p) == 108 )
		goto st64;
	goto st0;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	if ( (*p) == 121 )
		goto st65;
	goto st0;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	switch( (*p) ) {
		case 10: goto tr105;
		case 13: goto tr106;
		case 32: goto tr107;
	}
	goto st0;
tr107:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st66;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
#line 2160 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr95;
		case 13: goto st57;
		case 32: goto st66;
	}
	goto st0;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	if ( (*p) == 101 )
		goto st68;
	goto st0;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	switch( (*p) ) {
		case 99: goto st69;
		case 108: goto st85;
	}
	goto st0;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	if ( (*p) == 114 )
		goto st70;
	goto st0;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	if ( (*p) == 32 )
		goto tr113;
	goto st0;
tr113:
#line 562 "mod/box/memcached.rl"
	{incr_sign = -1;}
	goto st71;
tr202:
#line 561 "mod/box/memcached.rl"
	{incr_sign = 1; }
	goto st71;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
#line 2209 "mod/box/memcached.m"
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st71;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr114;
tr114:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st72;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
#line 2235 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st73;
	goto st0;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	if ( (*p) == 32 )
		goto st73;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr117;
	goto st0;
tr117:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st74;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
#line 2256 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr118;
		case 13: goto tr119;
		case 32: goto tr120;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st74;
	goto st0;
tr133:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st75;
tr119:
#line 515 "mod/box/memcached.rl"
	{incr = natoq(fstart, p);}
	goto st75;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
#line 2277 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr122;
	goto st0;
tr120:
#line 515 "mod/box/memcached.rl"
	{incr = natoq(fstart, p);}
	goto st76;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
#line 2289 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr122;
		case 13: goto st75;
		case 32: goto st76;
		case 110: goto st77;
	}
	goto st0;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	if ( (*p) == 111 )
		goto st78;
	goto st0;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
	if ( (*p) == 114 )
		goto st79;
	goto st0;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
	if ( (*p) == 101 )
		goto st80;
	goto st0;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
	if ( (*p) == 112 )
		goto st81;
	goto st0;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
	if ( (*p) == 108 )
		goto st82;
	goto st0;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
	if ( (*p) == 121 )
		goto st83;
	goto st0;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
	switch( (*p) ) {
		case 10: goto tr132;
		case 13: goto tr133;
		case 32: goto tr134;
	}
	goto st0;
tr134:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st84;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
#line 2357 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr122;
		case 13: goto st75;
		case 32: goto st84;
	}
	goto st0;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
	if ( (*p) == 101 )
		goto st86;
	goto st0;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
	if ( (*p) == 116 )
		goto st87;
	goto st0;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	if ( (*p) == 101 )
		goto st88;
	goto st0;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
	if ( (*p) == 32 )
		goto st89;
	goto st0;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st89;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr140;
tr140:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st90;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
#line 2421 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr141;
		case 13: goto st91;
		case 32: goto st92;
	}
	goto st0;
tr147:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st91;
tr158:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st91;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
#line 2444 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr141;
	goto st0;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
	switch( (*p) ) {
		case 10: goto tr141;
		case 13: goto st91;
		case 32: goto st92;
		case 110: goto st95;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr144;
	goto st0;
tr144:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st93;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
#line 2469 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr146;
		case 13: goto tr147;
		case 32: goto tr148;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st93;
	goto st0;
tr148:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st94;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
#line 2490 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr141;
		case 13: goto st91;
		case 32: goto st94;
		case 110: goto st95;
	}
	goto st0;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	if ( (*p) == 111 )
		goto st96;
	goto st0;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	if ( (*p) == 114 )
		goto st97;
	goto st0;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
	if ( (*p) == 101 )
		goto st98;
	goto st0;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
	if ( (*p) == 112 )
		goto st99;
	goto st0;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
	if ( (*p) == 108 )
		goto st100;
	goto st0;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
	if ( (*p) == 121 )
		goto st101;
	goto st0;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	switch( (*p) ) {
		case 10: goto tr157;
		case 13: goto tr158;
		case 32: goto tr159;
	}
	goto st0;
tr159:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st102;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
#line 2558 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr141;
		case 13: goto st91;
		case 32: goto st102;
	}
	goto st0;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
	if ( (*p) == 108 )
		goto st104;
	goto st0;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
	if ( (*p) == 117 )
		goto st105;
	goto st0;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	if ( (*p) == 115 )
		goto st106;
	goto st0;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
	if ( (*p) == 104 )
		goto st107;
	goto st0;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
	if ( (*p) == 95 )
		goto st108;
	goto st0;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
	if ( (*p) == 97 )
		goto st109;
	goto st0;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
	if ( (*p) == 108 )
		goto st110;
	goto st0;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
	if ( (*p) == 108 )
		goto st111;
	goto st0;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
	switch( (*p) ) {
		case 10: goto tr169;
		case 13: goto st112;
		case 32: goto st113;
	}
	goto st0;
tr186:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st112;
tr175:
#line 516 "mod/box/memcached.rl"
	{flush_delay = natoq(fstart, p);}
	goto st112;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
#line 2643 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr169;
	goto st0;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
	switch( (*p) ) {
		case 10: goto tr169;
		case 13: goto st112;
		case 32: goto st113;
		case 110: goto st116;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr172;
	goto st0;
tr172:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st114;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
#line 2668 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr174;
		case 13: goto tr175;
		case 32: goto tr176;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st114;
	goto st0;
tr176:
#line 516 "mod/box/memcached.rl"
	{flush_delay = natoq(fstart, p);}
	goto st115;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
#line 2685 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr169;
		case 13: goto st112;
		case 32: goto st115;
		case 110: goto st116;
	}
	goto st0;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
	if ( (*p) == 111 )
		goto st117;
	goto st0;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
	if ( (*p) == 114 )
		goto st118;
	goto st0;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
	if ( (*p) == 101 )
		goto st119;
	goto st0;
st119:
	if ( ++p == pe )
		goto _test_eof119;
case 119:
	if ( (*p) == 112 )
		goto st120;
	goto st0;
st120:
	if ( ++p == pe )
		goto _test_eof120;
case 120:
	if ( (*p) == 108 )
		goto st121;
	goto st0;
st121:
	if ( ++p == pe )
		goto _test_eof121;
case 121:
	if ( (*p) == 121 )
		goto st122;
	goto st0;
st122:
	if ( ++p == pe )
		goto _test_eof122;
case 122:
	switch( (*p) ) {
		case 10: goto tr185;
		case 13: goto tr186;
		case 32: goto tr187;
	}
	goto st0;
tr187:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st123;
st123:
	if ( ++p == pe )
		goto _test_eof123;
case 123:
#line 2753 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr169;
		case 13: goto st112;
		case 32: goto st123;
	}
	goto st0;
st124:
	if ( ++p == pe )
		goto _test_eof124;
case 124:
	if ( (*p) == 101 )
		goto st125;
	goto st0;
st125:
	if ( ++p == pe )
		goto _test_eof125;
case 125:
	if ( (*p) == 116 )
		goto st126;
	goto st0;
st126:
	if ( ++p == pe )
		goto _test_eof126;
case 126:
	switch( (*p) ) {
		case 32: goto tr191;
		case 115: goto st131;
	}
	goto st0;
tr191:
#line 558 "mod/box/memcached.rl"
	{show_cas = false;}
	goto st127;
tr198:
#line 559 "mod/box/memcached.rl"
	{show_cas = true;}
	goto st127;
st127:
	if ( ++p == pe )
		goto _test_eof127;
case 127:
#line 2795 "mod/box/memcached.m"
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st127;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr193;
tr193:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st128;
st128:
	if ( ++p == pe )
		goto _test_eof128;
case 128:
#line 2821 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr195;
		case 13: goto st129;
		case 32: goto st130;
	}
	goto st0;
st129:
	if ( ++p == pe )
		goto _test_eof129;
case 129:
	if ( (*p) == 10 )
		goto tr195;
	goto st0;
st130:
	if ( ++p == pe )
		goto _test_eof130;
case 130:
	switch( (*p) ) {
		case 9: goto st0;
		case 10: goto tr195;
		case 13: goto st129;
		case 32: goto st130;
	}
	goto tr193;
st131:
	if ( ++p == pe )
		goto _test_eof131;
case 131:
	if ( (*p) == 32 )
		goto tr198;
	goto st0;
st132:
	if ( ++p == pe )
		goto _test_eof132;
case 132:
	if ( (*p) == 110 )
		goto st133;
	goto st0;
st133:
	if ( ++p == pe )
		goto _test_eof133;
case 133:
	if ( (*p) == 99 )
		goto st134;
	goto st0;
st134:
	if ( ++p == pe )
		goto _test_eof134;
case 134:
	if ( (*p) == 114 )
		goto st135;
	goto st0;
st135:
	if ( ++p == pe )
		goto _test_eof135;
case 135:
	if ( (*p) == 32 )
		goto tr202;
	goto st0;
st136:
	if ( ++p == pe )
		goto _test_eof136;
case 136:
	if ( (*p) == 114 )
		goto st137;
	goto st0;
st137:
	if ( ++p == pe )
		goto _test_eof137;
case 137:
	if ( (*p) == 101 )
		goto st138;
	goto st0;
st138:
	if ( ++p == pe )
		goto _test_eof138;
case 138:
	if ( (*p) == 112 )
		goto st139;
	goto st0;
st139:
	if ( ++p == pe )
		goto _test_eof139;
case 139:
	if ( (*p) == 101 )
		goto st140;
	goto st0;
st140:
	if ( ++p == pe )
		goto _test_eof140;
case 140:
	if ( (*p) == 110 )
		goto st141;
	goto st0;
st141:
	if ( ++p == pe )
		goto _test_eof141;
case 141:
	if ( (*p) == 100 )
		goto st142;
	goto st0;
st142:
	if ( ++p == pe )
		goto _test_eof142;
case 142:
	if ( (*p) == 32 )
		goto tr209;
	goto st0;
st143:
	if ( ++p == pe )
		goto _test_eof143;
case 143:
	if ( (*p) == 117 )
		goto st144;
	goto st0;
st144:
	if ( ++p == pe )
		goto _test_eof144;
case 144:
	if ( (*p) == 105 )
		goto st145;
	goto st0;
st145:
	if ( ++p == pe )
		goto _test_eof145;
case 145:
	if ( (*p) == 116 )
		goto st146;
	goto st0;
st146:
	if ( ++p == pe )
		goto _test_eof146;
case 146:
	switch( (*p) ) {
		case 10: goto tr213;
		case 13: goto st147;
	}
	goto st0;
st147:
	if ( ++p == pe )
		goto _test_eof147;
case 147:
	if ( (*p) == 10 )
		goto tr213;
	goto st0;
st148:
	if ( ++p == pe )
		goto _test_eof148;
case 148:
	if ( (*p) == 101 )
		goto st149;
	goto st0;
st149:
	if ( ++p == pe )
		goto _test_eof149;
case 149:
	if ( (*p) == 112 )
		goto st150;
	goto st0;
st150:
	if ( ++p == pe )
		goto _test_eof150;
case 150:
	if ( (*p) == 108 )
		goto st151;
	goto st0;
st151:
	if ( ++p == pe )
		goto _test_eof151;
case 151:
	if ( (*p) == 97 )
		goto st152;
	goto st0;
st152:
	if ( ++p == pe )
		goto _test_eof152;
case 152:
	if ( (*p) == 99 )
		goto st153;
	goto st0;
st153:
	if ( ++p == pe )
		goto _test_eof153;
case 153:
	if ( (*p) == 101 )
		goto st154;
	goto st0;
st154:
	if ( ++p == pe )
		goto _test_eof154;
case 154:
	if ( (*p) == 32 )
		goto st155;
	goto st0;
st155:
	if ( ++p == pe )
		goto _test_eof155;
case 155:
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st155;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr222;
tr222:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st156;
st156:
	if ( ++p == pe )
		goto _test_eof156;
case 156:
#line 3045 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st157;
	goto st0;
st157:
	if ( ++p == pe )
		goto _test_eof157;
case 157:
	if ( (*p) == 32 )
		goto st157;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr224;
	goto st0;
tr224:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st158;
st158:
	if ( ++p == pe )
		goto _test_eof158;
case 158:
#line 3066 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr225;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st158;
	goto st0;
tr225:
#line 512 "mod/box/memcached.rl"
	{flags = natoq(fstart, p);}
	goto st159;
st159:
	if ( ++p == pe )
		goto _test_eof159;
case 159:
#line 3080 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st159;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr228;
	goto st0;
tr228:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st160;
st160:
	if ( ++p == pe )
		goto _test_eof160;
case 160:
#line 3094 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr229;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st160;
	goto st0;
tr229:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st161;
st161:
	if ( ++p == pe )
		goto _test_eof161;
case 161:
#line 3112 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st161;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr232;
	goto st0;
tr232:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st162;
st162:
	if ( ++p == pe )
		goto _test_eof162;
case 162:
#line 3126 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr233;
		case 13: goto tr234;
		case 32: goto tr235;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st162;
	goto st0;
tr234:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st163;
tr247:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st163;
st163:
	if ( ++p == pe )
		goto _test_eof163;
case 163:
#line 3147 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr237;
	goto st0;
tr235:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st164;
st164:
	if ( ++p == pe )
		goto _test_eof164;
case 164:
#line 3159 "mod/box/memcached.m"
	switch( (*p) ) {
		case 32: goto st164;
		case 110: goto st165;
	}
	goto st0;
st165:
	if ( ++p == pe )
		goto _test_eof165;
case 165:
	if ( (*p) == 111 )
		goto st166;
	goto st0;
st166:
	if ( ++p == pe )
		goto _test_eof166;
case 166:
	if ( (*p) == 114 )
		goto st167;
	goto st0;
st167:
	if ( ++p == pe )
		goto _test_eof167;
case 167:
	if ( (*p) == 101 )
		goto st168;
	goto st0;
st168:
	if ( ++p == pe )
		goto _test_eof168;
case 168:
	if ( (*p) == 112 )
		goto st169;
	goto st0;
st169:
	if ( ++p == pe )
		goto _test_eof169;
case 169:
	if ( (*p) == 108 )
		goto st170;
	goto st0;
st170:
	if ( ++p == pe )
		goto _test_eof170;
case 170:
	if ( (*p) == 121 )
		goto st171;
	goto st0;
st171:
	if ( ++p == pe )
		goto _test_eof171;
case 171:
	switch( (*p) ) {
		case 10: goto tr246;
		case 13: goto tr247;
	}
	goto st0;
st172:
	if ( ++p == pe )
		goto _test_eof172;
case 172:
	switch( (*p) ) {
		case 101: goto st173;
		case 116: goto st192;
	}
	goto st0;
st173:
	if ( ++p == pe )
		goto _test_eof173;
case 173:
	if ( (*p) == 116 )
		goto st174;
	goto st0;
st174:
	if ( ++p == pe )
		goto _test_eof174;
case 174:
	if ( (*p) == 32 )
		goto st175;
	goto st0;
st175:
	if ( ++p == pe )
		goto _test_eof175;
case 175:
	switch( (*p) ) {
		case 13: goto st0;
		case 32: goto st175;
	}
	if ( 9 <= (*p) && (*p) <= 10 )
		goto st0;
	goto tr252;
tr252:
#line 489 "mod/box/memcached.rl"
	{
			fstart = p;
			for (; p < pe && *p != ' ' && *p != '\r' && *p != '\n'; p++);
			if ( *p == ' ' || *p == '\r' || *p == '\n') {
				write_varint32(keys, p - fstart);
				tbuf_append(keys, fstart, p - fstart);
				keys_count++;
				p--;
			} else
				p = fstart;
 		}
	goto st176;
st176:
	if ( ++p == pe )
		goto _test_eof176;
case 176:
#line 3268 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st177;
	goto st0;
st177:
	if ( ++p == pe )
		goto _test_eof177;
case 177:
	if ( (*p) == 32 )
		goto st177;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr254;
	goto st0;
tr254:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st178;
st178:
	if ( ++p == pe )
		goto _test_eof178;
case 178:
#line 3289 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr255;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st178;
	goto st0;
tr255:
#line 512 "mod/box/memcached.rl"
	{flags = natoq(fstart, p);}
	goto st179;
st179:
	if ( ++p == pe )
		goto _test_eof179;
case 179:
#line 3303 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st179;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr258;
	goto st0;
tr258:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st180;
st180:
	if ( ++p == pe )
		goto _test_eof180;
case 180:
#line 3317 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto tr259;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st180;
	goto st0;
tr259:
#line 505 "mod/box/memcached.rl"
	{
			exptime = natoq(fstart, p);
			if (exptime > 0 && exptime <= 60*60*24*30)
				exptime = exptime + ev_now();
		}
	goto st181;
st181:
	if ( ++p == pe )
		goto _test_eof181;
case 181:
#line 3335 "mod/box/memcached.m"
	if ( (*p) == 32 )
		goto st181;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr262;
	goto st0;
tr262:
#line 488 "mod/box/memcached.rl"
	{ fstart = p; }
	goto st182;
st182:
	if ( ++p == pe )
		goto _test_eof182;
case 182:
#line 3349 "mod/box/memcached.m"
	switch( (*p) ) {
		case 10: goto tr263;
		case 13: goto tr264;
		case 32: goto tr265;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st182;
	goto st0;
tr264:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st183;
tr277:
#line 547 "mod/box/memcached.rl"
	{ noreply = true; }
	goto st183;
st183:
	if ( ++p == pe )
		goto _test_eof183;
case 183:
#line 3370 "mod/box/memcached.m"
	if ( (*p) == 10 )
		goto tr267;
	goto st0;
tr265:
#line 513 "mod/box/memcached.rl"
	{bytes = natoq(fstart, p);}
	goto st184;
st184:
	if ( ++p == pe )
		goto _test_eof184;
case 184:
#line 3382 "mod/box/memcached.m"
	switch( (*p) ) {
		case 32: goto st184;
		case 110: goto st185;
	}
	goto st0;
st185:
	if ( ++p == pe )
		goto _test_eof185;
case 185:
	if ( (*p) == 111 )
		goto st186;
	goto st0;
st186:
	if ( ++p == pe )
		goto _test_eof186;
case 186:
	if ( (*p) == 114 )
		goto st187;
	goto st0;
st187:
	if ( ++p == pe )
		goto _test_eof187;
case 187:
	if ( (*p) == 101 )
		goto st188;
	goto st0;
st188:
	if ( ++p == pe )
		goto _test_eof188;
case 188:
	if ( (*p) == 112 )
		goto st189;
	goto st0;
st189:
	if ( ++p == pe )
		goto _test_eof189;
case 189:
	if ( (*p) == 108 )
		goto st190;
	goto st0;
st190:
	if ( ++p == pe )
		goto _test_eof190;
case 190:
	if ( (*p) == 121 )
		goto st191;
	goto st0;
st191:
	if ( ++p == pe )
		goto _test_eof191;
case 191:
	switch( (*p) ) {
		case 10: goto tr276;
		case 13: goto tr277;
	}
	goto st0;
st192:
	if ( ++p == pe )
		goto _test_eof192;
case 192:
	if ( (*p) == 97 )
		goto st193;
	goto st0;
st193:
	if ( ++p == pe )
		goto _test_eof193;
case 193:
	if ( (*p) == 116 )
		goto st194;
	goto st0;
st194:
	if ( ++p == pe )
		goto _test_eof194;
case 194:
	if ( (*p) == 115 )
		goto st195;
	goto st0;
st195:
	if ( ++p == pe )
		goto _test_eof195;
case 195:
	switch( (*p) ) {
		case 10: goto tr281;
		case 13: goto st196;
	}
	goto st0;
st196:
	if ( ++p == pe )
		goto _test_eof196;
case 196:
	if ( (*p) == 10 )
		goto tr281;
	goto st0;
	}
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof197: cs = 197; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 
	_test_eof62: cs = 62; goto _test_eof; 
	_test_eof63: cs = 63; goto _test_eof; 
	_test_eof64: cs = 64; goto _test_eof; 
	_test_eof65: cs = 65; goto _test_eof; 
	_test_eof66: cs = 66; goto _test_eof; 
	_test_eof67: cs = 67; goto _test_eof; 
	_test_eof68: cs = 68; goto _test_eof; 
	_test_eof69: cs = 69; goto _test_eof; 
	_test_eof70: cs = 70; goto _test_eof; 
	_test_eof71: cs = 71; goto _test_eof; 
	_test_eof72: cs = 72; goto _test_eof; 
	_test_eof73: cs = 73; goto _test_eof; 
	_test_eof74: cs = 74; goto _test_eof; 
	_test_eof75: cs = 75; goto _test_eof; 
	_test_eof76: cs = 76; goto _test_eof; 
	_test_eof77: cs = 77; goto _test_eof; 
	_test_eof78: cs = 78; goto _test_eof; 
	_test_eof79: cs = 79; goto _test_eof; 
	_test_eof80: cs = 80; goto _test_eof; 
	_test_eof81: cs = 81; goto _test_eof; 
	_test_eof82: cs = 82; goto _test_eof; 
	_test_eof83: cs = 83; goto _test_eof; 
	_test_eof84: cs = 84; goto _test_eof; 
	_test_eof85: cs = 85; goto _test_eof; 
	_test_eof86: cs = 86; goto _test_eof; 
	_test_eof87: cs = 87; goto _test_eof; 
	_test_eof88: cs = 88; goto _test_eof; 
	_test_eof89: cs = 89; goto _test_eof; 
	_test_eof90: cs = 90; goto _test_eof; 
	_test_eof91: cs = 91; goto _test_eof; 
	_test_eof92: cs = 92; goto _test_eof; 
	_test_eof93: cs = 93; goto _test_eof; 
	_test_eof94: cs = 94; goto _test_eof; 
	_test_eof95: cs = 95; goto _test_eof; 
	_test_eof96: cs = 96; goto _test_eof; 
	_test_eof97: cs = 97; goto _test_eof; 
	_test_eof98: cs = 98; goto _test_eof; 
	_test_eof99: cs = 99; goto _test_eof; 
	_test_eof100: cs = 100; goto _test_eof; 
	_test_eof101: cs = 101; goto _test_eof; 
	_test_eof102: cs = 102; goto _test_eof; 
	_test_eof103: cs = 103; goto _test_eof; 
	_test_eof104: cs = 104; goto _test_eof; 
	_test_eof105: cs = 105; goto _test_eof; 
	_test_eof106: cs = 106; goto _test_eof; 
	_test_eof107: cs = 107; goto _test_eof; 
	_test_eof108: cs = 108; goto _test_eof; 
	_test_eof109: cs = 109; goto _test_eof; 
	_test_eof110: cs = 110; goto _test_eof; 
	_test_eof111: cs = 111; goto _test_eof; 
	_test_eof112: cs = 112; goto _test_eof; 
	_test_eof113: cs = 113; goto _test_eof; 
	_test_eof114: cs = 114; goto _test_eof; 
	_test_eof115: cs = 115; goto _test_eof; 
	_test_eof116: cs = 116; goto _test_eof; 
	_test_eof117: cs = 117; goto _test_eof; 
	_test_eof118: cs = 118; goto _test_eof; 
	_test_eof119: cs = 119; goto _test_eof; 
	_test_eof120: cs = 120; goto _test_eof; 
	_test_eof121: cs = 121; goto _test_eof; 
	_test_eof122: cs = 122; goto _test_eof; 
	_test_eof123: cs = 123; goto _test_eof; 
	_test_eof124: cs = 124; goto _test_eof; 
	_test_eof125: cs = 125; goto _test_eof; 
	_test_eof126: cs = 126; goto _test_eof; 
	_test_eof127: cs = 127; goto _test_eof; 
	_test_eof128: cs = 128; goto _test_eof; 
	_test_eof129: cs = 129; goto _test_eof; 
	_test_eof130: cs = 130; goto _test_eof; 
	_test_eof131: cs = 131; goto _test_eof; 
	_test_eof132: cs = 132; goto _test_eof; 
	_test_eof133: cs = 133; goto _test_eof; 
	_test_eof134: cs = 134; goto _test_eof; 
	_test_eof135: cs = 135; goto _test_eof; 
	_test_eof136: cs = 136; goto _test_eof; 
	_test_eof137: cs = 137; goto _test_eof; 
	_test_eof138: cs = 138; goto _test_eof; 
	_test_eof139: cs = 139; goto _test_eof; 
	_test_eof140: cs = 140; goto _test_eof; 
	_test_eof141: cs = 141; goto _test_eof; 
	_test_eof142: cs = 142; goto _test_eof; 
	_test_eof143: cs = 143; goto _test_eof; 
	_test_eof144: cs = 144; goto _test_eof; 
	_test_eof145: cs = 145; goto _test_eof; 
	_test_eof146: cs = 146; goto _test_eof; 
	_test_eof147: cs = 147; goto _test_eof; 
	_test_eof148: cs = 148; goto _test_eof; 
	_test_eof149: cs = 149; goto _test_eof; 
	_test_eof150: cs = 150; goto _test_eof; 
	_test_eof151: cs = 151; goto _test_eof; 
	_test_eof152: cs = 152; goto _test_eof; 
	_test_eof153: cs = 153; goto _test_eof; 
	_test_eof154: cs = 154; goto _test_eof; 
	_test_eof155: cs = 155; goto _test_eof; 
	_test_eof156: cs = 156; goto _test_eof; 
	_test_eof157: cs = 157; goto _test_eof; 
	_test_eof158: cs = 158; goto _test_eof; 
	_test_eof159: cs = 159; goto _test_eof; 
	_test_eof160: cs = 160; goto _test_eof; 
	_test_eof161: cs = 161; goto _test_eof; 
	_test_eof162: cs = 162; goto _test_eof; 
	_test_eof163: cs = 163; goto _test_eof; 
	_test_eof164: cs = 164; goto _test_eof; 
	_test_eof165: cs = 165; goto _test_eof; 
	_test_eof166: cs = 166; goto _test_eof; 
	_test_eof167: cs = 167; goto _test_eof; 
	_test_eof168: cs = 168; goto _test_eof; 
	_test_eof169: cs = 169; goto _test_eof; 
	_test_eof170: cs = 170; goto _test_eof; 
	_test_eof171: cs = 171; goto _test_eof; 
	_test_eof172: cs = 172; goto _test_eof; 
	_test_eof173: cs = 173; goto _test_eof; 
	_test_eof174: cs = 174; goto _test_eof; 
	_test_eof175: cs = 175; goto _test_eof; 
	_test_eof176: cs = 176; goto _test_eof; 
	_test_eof177: cs = 177; goto _test_eof; 
	_test_eof178: cs = 178; goto _test_eof; 
	_test_eof179: cs = 179; goto _test_eof; 
	_test_eof180: cs = 180; goto _test_eof; 
	_test_eof181: cs = 181; goto _test_eof; 
	_test_eof182: cs = 182; goto _test_eof; 
	_test_eof183: cs = 183; goto _test_eof; 
	_test_eof184: cs = 184; goto _test_eof; 
	_test_eof185: cs = 185; goto _test_eof; 
	_test_eof186: cs = 186; goto _test_eof; 
	_test_eof187: cs = 187; goto _test_eof; 
	_test_eof188: cs = 188; goto _test_eof; 
	_test_eof189: cs = 189; goto _test_eof; 
	_test_eof190: cs = 190; goto _test_eof; 
	_test_eof191: cs = 191; goto _test_eof; 
	_test_eof192: cs = 192; goto _test_eof; 
	_test_eof193: cs = 193; goto _test_eof; 
	_test_eof194: cs = 194; goto _test_eof; 
	_test_eof195: cs = 195; goto _test_eof; 
	_test_eof196: cs = 196; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}

#line 572 "mod/box/memcached.rl"


	if (!done) {
		say_debug("parse failed after: `%.*s'", (int)(pe - p), p);
		if (pe - p > (1 << 20)) {
		exit:
			say_warn("memcached proto error");
			add_iov("ERROR\r\n", 7);
			stats.bytes_written += 7;
			return -1;
		}
		char *r;
		if ((r = memmem(p, pe - p, "\r\n", 2)) != NULL) {
			tbuf_peek(fiber->rbuf, r + 2 - (char *)fiber->rbuf->data);
			add_iov("CLIENT_ERROR bad command line format\r\n", 38);
			return 1;
		}
		return 0;
	}

	if (noreply) {
		fiber->iov_cnt = saved_iov_cnt;
		fiber->iov->len = saved_iov_cnt * sizeof(struct iovec);
	}

	return 1;
}

void
memcached_handler(void *_data __attribute__((unused)))
{
	struct box_txn *txn;
	stats.total_connections++;
	stats.curr_connections++;
	int r, p;
	int batch_count;

	for (;;) {
		batch_count = 0;
		if ((r = fiber_bread(fiber->rbuf, 1)) <= 0) {
			say_debug("read returned %i, closing connection", r);
			goto exit;
		}

	dispatch:
		txn = txn_alloc(BOX_QUIET);
		p = memcached_dispatch(txn);
		if (p < 0) {
			say_debug("negative dispatch, closing connection");
			goto exit;
		}

		if (p == 0 && batch_count == 0) /* we havn't successfully parsed any requests */
			continue;

		if (p == 1) {
			batch_count++;
			/* some unparsed commands remain and batch count less than 20 */
			if (fiber->rbuf->len > 0 && batch_count < 20)
				goto dispatch;
		}

		r = fiber_flush_output();
		if (r < 0) {
			say_debug("flush_output failed, closing connection");
			goto exit;
		}

		stats.bytes_written += r;
		fiber_gc();

		if (p == 1 && fiber->rbuf->len > 0) {
			batch_count = 0;
			goto dispatch;
		}
	}
exit:
        fiber_flush_output();
	fiber_sleep(0.01);
	say_debug("exit");
	stats.curr_connections--; /* FIXME: nonlocal exit via exception will leak this counter */
}

void
memcached_init(void)
{
	stat_base = stat_register(memcached_stat_strs, memcached_stat_MAX);
}

void
memcached_expire(void *data __attribute__((unused)))
{
	static khiter_t i;
	khash_t(lstr_ptr_map) *map = memcached_index->idx.str_hash;

	say_info("memcached expire fiber started");
	for (;;) {
		if (i > kh_end(map))
			i = kh_begin(map);

		struct tbuf *keys_to_delete = tbuf_alloc(fiber->pool);
		int expired_keys = 0;

		for (int j = 0; j < cfg.memcached_expire_per_loop; j++, i++) {
			if (i == kh_end(map)) {
				i = kh_begin(map);
				break;
			}

			if (!kh_exist(map, i))
				continue;

			struct box_tuple *tuple = kh_value(map, i);

			if (!expired(tuple))
				continue;

			say_debug("expire tuple %p", tuple);
			tbuf_append_field(keys_to_delete, tuple->data);
		}

		while (keys_to_delete->len > 0) {
			struct box_txn *txn = txn_alloc(BOX_QUIET);
			delete(txn, read_field(keys_to_delete));
			expired_keys++;
		}
		stat_collect(stat_base, MEMC_EXPIRED_KEYS, expired_keys);

		fiber_gc();

		double delay = (double)cfg.memcached_expire_per_loop * cfg.memcached_expire_full_sweep / (map->size + 1);
		if (delay > 1)
			delay = 1;
		fiber_sleep(delay);
	}
}

/*
 * Local Variables:
 * mode: c
 * End:
 * vim: syntax=c
 */
