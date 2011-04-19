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

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

#include <third_party/luajit/src/lua.h>
#include <third_party/luajit/src/lauxlib.h>

#include <palloc.h>
#include <pickle.h>
#include <tbuf.h>
#include <util.h>
#include <fiber.h>

#ifdef POISON
#  define TBUF_POISON
#endif

#ifdef TBUF_POISON
#  define poison(ptr, len) memset((ptr), 'A', (len))
#else
#  define poison(ptr, len)
#endif

static void
tbuf_assert(const struct tbuf *b)
{
	(void)b;		/* arg used :-) */
	assert(b->len <= b->size);
}

struct tbuf *
tbuf_alloc(struct palloc_pool *pool)
{
	const size_t initial_size = 128 - sizeof(struct tbuf);
	struct tbuf *e = palloc(pool, sizeof(*e) + initial_size);
	e->len = 0;
	e->size = initial_size;
	e->data = (char *)e + sizeof(*e);
	e->pool = pool;
	poison(e->data, e->size);
	tbuf_assert(e);
	return e;
}

struct tbuf *
tbuf_alloc_fixed(struct palloc_pool *pool, void *data, size_t len)
{
	struct tbuf *e = palloc(pool, sizeof(*e));
	e->pool = NULL;
	e->len = 0;
	e->size = len;
	e->data = data;
	return e;
}


void
tbuf_ensure_resize(struct tbuf *e, size_t required)
{
	tbuf_assert(e);

	assert(e->pool != NULL); /* attemp to resize fixed size tbuf */

	const size_t initial_size = MAX(e->size, 128 - sizeof(*e));
	size_t new_size = initial_size * 2;

	while (new_size - e->len < required)
		new_size *= 2;

	void *p = palloc(e->pool, new_size);

	poison(p, new_size);
	memcpy(p, e->data, e->size);
	poison(e->data, e->len);
	e->data = p;
	e->size = new_size;
	tbuf_assert(e);
}

struct tbuf *
tbuf_clone(struct palloc_pool *pool, const struct tbuf *orig)
{
	struct tbuf *clone = tbuf_alloc(pool);
	tbuf_assert(orig);
	tbuf_append(clone, orig->data, orig->len);
	return clone;
}

struct tbuf *
tbuf_split(struct tbuf *orig, size_t at)
{
	tbuf_assert(orig);
	assert(at <= orig->len);

	struct tbuf *head = tbuf_alloc_fixed(orig->pool, orig->data, at);
	orig->data += at;
	orig->size -= at;
	orig->len -= at;
	head->len += at;
	return head;
}

void *
tbuf_peek(struct tbuf *b, size_t count)
{
	void *p = b->data;
	tbuf_assert(b);
	if (count <= b->len) {
		b->data += count;
		b->len -= count;
		b->size -= count;
		return p;
	}
	return NULL;
}

size_t
tbuf_reserve(struct tbuf *b, size_t count)
{
	tbuf_assert(b);
	tbuf_ensure(b, count);
	size_t offt = b->len;
	b->len += count;
	return offt;
}

void
tbuf_reset(struct tbuf *b)
{
	tbuf_assert(b);
	poison(b->data, b->len);
	b->len = 0;
}

void
tbuf_append_field(struct tbuf *b, void *f)
{
	void *s = f;
	u32 size = load_varint32(&f);
	void *next = (u8 *)f + size;
	tbuf_append(b, s, next - s);
}

void
tbuf_vprintf(struct tbuf *b, const char *format, va_list ap)
{
	int printed_len;
	size_t free_len = b->size - b->len;
	va_list ap_copy;

	va_copy(ap_copy, ap);

	tbuf_assert(b);
	printed_len = vsnprintf(((char *)b->data) + b->len, free_len, format, ap);

	/*
	 * if buffer too short, resize buffer and
	 * print it again
	 */
	if (free_len <= printed_len) {
		tbuf_ensure(b, printed_len + 1);
		free_len = b->size - b->len - 1;
		printed_len = vsnprintf(((char *)b->data) + b->len, free_len, format, ap_copy);
	}

	b->len += printed_len;

	va_end(ap_copy);
}

void
tbuf_printf(struct tbuf *b, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	tbuf_vprintf(b, format, args);
	va_end(args);
}

/* for debug printing */
char *
tbuf_to_hex(const struct tbuf *x)
{
	const unsigned char *data = x->data;
	size_t len = x->len;
	char *out = palloc(x->pool, len * 3 + 1);
	out[len * 3] = 0;

	for (int i = 0; i < len; i++) {
		int c = *(data + i);
		sprintf(out + i * 3, "%02x ", c);
	}

	return out;
}


/* lua support */
const char *tbuflib_name = "Tarantool.tbuf";

int
luaT_pushtbuf(struct lua_State *L, struct tbuf *orig)
{
	struct tbuf **b = lua_newuserdata(L, sizeof(struct tbuf *));
	*b = orig;
	luaL_getmetatable(L, tbuflib_name);
	lua_setmetatable(L, -2);
	return 1;
}

struct tbuf *
luaT_checktbuf(struct lua_State *L, int idx)
{
	struct tbuf **b = luaL_checkudata(L, idx, tbuflib_name);
	assert(b != NULL);
	return *b;
}

static int
luaT_tbuf_len(struct lua_State *L)
{
	struct tbuf *b = luaT_checktbuf(L, 1);
	lua_pushinteger(L, b->len);
	return 1;
}

static int
luaT_tbuf_tostring(struct lua_State *L)
{
	struct tbuf *b = luaT_checktbuf(L, 1);
	lua_pushlstring(L, b->data, b->len);
	return 1;
}

static int
luaT_tbuf_append(struct lua_State *L)
{
	struct tbuf *b = luaT_checktbuf(L, 1);
	const char *format = luaL_checkstring(L, 2);
	u32 u32;
	u64 u64;
	const char *str;
	int i = 3; /* first arg comes third */

	while (*format) {
		switch (*format) {
		case 'u':
			u32 = luaL_checkinteger(L, i);
			tbuf_append(b, &u32, sizeof(u32));
			break;
		case 'l':
			u64 = luaL_checkinteger(L, i);
			tbuf_append(b, &u64, sizeof(u64));
			break;
		case 'w':
			u32 = luaL_checkinteger(L, i);
			write_varint32(b, u32);
			break;
		case 's':
			str = luaL_checklstring(L, i, &u32);
			tbuf_append(b, str, u32);
			break;
		case 'f':
			str = luaL_checklstring(L, i, &u32);
			write_varint32(b, u32);
			tbuf_append(b, str, u32);
			break;
		default:
			lua_pushliteral(L, "bad pack format");
			lua_error(L);
		}
		i++;
		format++;
	}
	return 0;
}


static int
luaT_tbuf_alloc(struct lua_State *L)
{
	return luaT_pushtbuf(L, tbuf_alloc(fiber->pool));
}

static int
luaT_tbuf_reserve(struct lua_State *L)
{
	struct tbuf *b = luaT_checktbuf(L, 1);
	i64 len = luaL_checkinteger(L, 2);

	if (len <= 0) {
		lua_pushliteral(L, "len must be greater then 0");
		lua_error(L);
	}

	tbuf_reserve(b, len);
	return 0;
}

static int
luaT_tbuf_add_iov(struct lua_State *L)
{
	struct tbuf *b = luaT_checktbuf(L, 1);
	i64 len = luaL_checkinteger(L, 2);

	if (len <= 0) {
		lua_pushliteral(L, "len must be greater then 0");
		lua_error(L);
	}

	b->pool = NULL; /* tbuf is fixed from now */
	add_iov(b->data, len);
	return 0;
}

static int
luaT_tbuf_alloc_fixed(struct lua_State *L)
{
	u32 len = luaL_checkinteger(L, 1);
	void *ptr = palloc(fiber->pool, len);
	return luaT_pushtbuf(L, tbuf_alloc_fixed(fiber->pool, ptr, len));
}

static const struct luaL_reg tbuflib_m [] = {
	{"#", luaT_tbuf_len},
	{"__tostring", luaT_tbuf_tostring},
	{NULL, NULL}
};

static const struct luaL_reg tbuflib [] = {
	{"append", luaT_tbuf_append},
	{"reserve", luaT_tbuf_reserve},
	{"add_iov", luaT_tbuf_add_iov},
	{"alloc", luaT_tbuf_alloc},
	{"alloc_fixed", luaT_tbuf_alloc_fixed},
	{NULL, NULL}
};

int
luaT_opentbuf(struct lua_State *L)
{
	luaL_newmetatable(L, tbuflib_name);
	luaL_register(L, NULL, tbuflib_m);
	luaL_register(L, "tbuf", tbuflib);
	return 0;
}
