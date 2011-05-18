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

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>


#include <fiber.h>
#include <say.h>

#include <cfg/tarantool_box_cfg.h>
#include <mod/box/box.h>
#include <mod/box/index.h>

#include <third_party/luajit/src/lua.h>
#include <third_party/luajit/src/lualib.h>
#include <third_party/luajit/src/lauxlib.h>

const char *tuplelib_name = "Tarantool.box.tuple";

void
luaT_pushtuple(struct lua_State *L, struct box_tuple *tuple)
{
	void **ptr = lua_newuserdata(L, sizeof(void *));
	luaL_getmetatable(L, tuplelib_name);
	lua_setmetatable(L, -2);
	*ptr = tuple;
	tuple_ref(tuple, 1);
}

static int
tuple_gc_(struct lua_State *L)
{
	struct box_tuple *tuple = *(void **)luaL_checkudata(L, 1, tuplelib_name);
	tuple_ref(tuple, -1);
	return 0;
}

static int
tuple_len_(struct lua_State *L)
{
	struct box_tuple *tuple = *(void **)luaL_checkudata(L, 1, tuplelib_name);
	lua_pushnumber(L, tuple->cardinality);
	return 1;
}

static int
tuple_index_(struct lua_State *L)
{
	struct box_tuple *tuple = *(void **)luaL_checkudata(L, 1, tuplelib_name);
	int i = luaL_checkint(L, 2);
	if (i >= tuple->cardinality) {
		lua_pushliteral(L, "index too small");
		lua_error(L);
	}

	void *field = tuple_field(tuple, i);
	u32 len = load_varint32(&field);
	lua_pushlstring(L, field, len);
	return 1;
}


struct box_tuple *
luaT_toboxtuple(struct lua_State *L, int table)
{
	luaL_checktype(L, table, LUA_TTABLE);

	u32 bsize = 0, cardinality = lua_objlen(L, table);

	for (int i = 0; i < cardinality; i++) {
		lua_rawgeti(L, table, i + 1);
		u32 len = lua_objlen(L, -1);
		lua_pop(L, 1);
		bsize += varint32_sizeof(len) + len;
	}

	struct box_tuple *tuple = tuple_alloc(bsize);
	tuple->cardinality = cardinality;

	u8 *p = tuple->data;
	for (int i = 0; i < cardinality; i++) {
		lua_rawgeti(L, table, i + 1);
		u32 len;
		const char *str = lua_tolstring(L, -1, &len);
		lua_pop(L, 1);

		p = save_varint32(p, len);
		memcpy(p, str, len);
		p += len;
	}

	return tuple;
}

static const struct luaL_reg tuplelib [] = {
	{"__len", tuple_len_},
	{"__index", tuple_index_},
	{"__gc", tuple_gc_},
	{NULL, NULL}
};


const char *txnlib_name = "Tarantool.box.txn";

void
luaT_pushtxn(struct lua_State *L, struct box_txn *txn)
{
	void **ptr = lua_newuserdata(L, sizeof(void *));
	luaL_getmetatable(L, txnlib_name);
	lua_setmetatable(L, -2);
	*ptr = txn;
}

static struct box_txn *
luaT_checktxn(struct lua_State *L, int i)
{
	struct box_txn **txn = luaL_checkudata(L, i, txnlib_name);
	assert(index != NULL);
	return *txn;
}

static int
txn_alloc_(struct lua_State *L)
{
	luaT_pushtxn(L, txn_alloc(BOX_QUIET));
	return 1;
}


static const struct luaL_reg txnlib [] = {
	{"alloc", txn_alloc_},
	{NULL, NULL}
};


const char *indexlib_name = "Tarantool.box.index";

static void
luaT_pushindex(struct lua_State *L, struct index *index)
{
	void **ptr = lua_newuserdata(L, sizeof(void *));
	luaL_getmetatable(L, indexlib_name);
	lua_setmetatable(L, -2);
	*ptr = index;
}

static struct index*
luaT_checkindex(struct lua_State *L, int i)
{
	struct index **index = luaL_checkudata(L, i, indexlib_name);
	assert(index != NULL);
	return *index;
}

static int
luaT_index_index(struct lua_State *L)
{
	struct index *index = luaT_checkindex(L, 1);

	struct tbuf *key;
	if (lua_isstring(L, 2)) {
		key = tbuf_alloc(fiber->pool);
		size_t len;
		const char *str = lua_tolstring(L, 2, &len);
		write_varint32(key, len);
		tbuf_append(key, str, len);
	} else {
		key = luaT_checktbuf(L, 2);
	}

	void *tuple = index->find(index, key->data);
	if (tuple != NULL) {
		luaT_pushtuple(L, tuple);
		return 1;
	}
	return 0;
}

static int
luaT_index_hashget(struct lua_State *L)
{
	struct index *index = luaT_checkindex(L, 1);
	i64 i = luaL_checkinteger(L, 2);

	if (index->type != HASH) {
		lua_pushliteral(L, "index type must be hash");
		lua_error(L);
	}

	khash_t(lstr_ptr_map) *map = index->idx.str_hash;

	if (i < kh_begin(map) || i > kh_end(map))
		return 0;

	if (!kh_exist(map, i))
		return 0;

	struct box_tuple *tuple = kh_value(map, i);
	luaT_pushtuple(L, tuple);
	return 1;
}

static int
luaT_index_hashsize(struct lua_State *L)
{
	struct index *index = luaT_checkindex(L, 1);

	if (index->type != HASH) {
		lua_pushliteral(L, "index type must be hash");
		lua_error(L);
	}

	khash_t(lstr_ptr_map) *map = index->idx.str_hash;

	lua_pushinteger(L, kh_end(map));
	return 1;
}

static int
luaT_index_hashnext(struct lua_State *L)
{
	struct index *index = luaT_checkindex(L, 1);
	i64 i;

	if (lua_isnil(L, 2))
		i = 0;
	else
		i = luaL_checkinteger(L, 2) + 1;

	if (index->type != HASH) {
		lua_pushliteral(L, "index type must be hash");
		lua_error(L);
	}

	khash_t(lstr_ptr_map) *map = index->idx.str_hash;

	if (i < kh_begin(map) || i > kh_end(map) || map->keys == NULL)
		return 0;

	do {
		if (kh_exist(map, i)) {
			struct box_tuple *tuple = kh_value(map, i);
			lua_pushinteger(L, i);
			luaT_pushtuple(L, tuple);
			return 2;
		}
	} while (++i < kh_end(map));

	return 0;
}

static const struct luaL_reg indexlib_m [] = {
	{"__index", luaT_index_index},
	{NULL, NULL}
};

static const struct luaL_reg indexlib [] = {
	{"hashget", luaT_index_hashget},
	{"hashsize", luaT_index_hashsize},
	{"hashnext", luaT_index_hashnext},
	{NULL, NULL}
};



static int
luaT_tuple_add_iov(struct lua_State *L)
{
	struct box_txn *txn = luaT_checktxn(L, 1);
	struct box_tuple *tuple = *(void **)luaL_checkudata(L, 2, tuplelib_name);

	tuple_add_iov(txn, tuple);
	return 0;
}

static int
luaT_box_dispatch(struct lua_State *L)
{
	struct box_txn *txn = luaT_checktxn(L, 1);
	u32 op = luaL_checkinteger(L, 2);
	struct tbuf *req = luaT_checktbuf(L, 3);
	u32 r = box_process(txn, op, req);
	lua_pushinteger(L, r);
	return 1;
}


static const struct luaL_reg boxlib [] = {
	{"tuple_add_iov", luaT_tuple_add_iov},
	{"dispatch", luaT_box_dispatch},
	{NULL, NULL}
};


void
luaT_openbox(struct lua_State *L)
{
        lua_getglobal(L, "require");
        lua_pushliteral(L, "box_prelude");
	if (lua_pcall(L, 1, 0, 0) != 0)
		panic("moonbox: %s", lua_tostring(L, -1));

	luaL_newmetatable(L, indexlib_name);
	luaL_register(L, NULL, indexlib_m);
	luaL_register(L, "box.index", indexlib);

	luaL_newmetatable(L, tuplelib_name);
	luaL_register(L, NULL, tuplelib);

	luaL_newmetatable(L, txnlib_name);
	lua_pop(L, 1); /* there are no metamethods in txnlib */
	luaL_register(L, "box.txn", txnlib);

	luaL_register(L, "box", boxlib);

	lua_createtable(L, 0, 0); /* namespace_registry */
	for (uint32_t n = 0; n < namespace_count; ++n) {
		if (!namespace[n].enabled)
			continue;

		lua_createtable(L, 0, 0); /* namespace */
		lua_pushliteral(L, "index");
		lua_createtable(L, 0, 0); /* index */
		for (int i = 0; i < MAX_IDX; i++) {
			struct index *index = &namespace[n].index[i];
			if (index->key_cardinality == 0)
				break;
			luaT_pushindex(L, index);
			lua_rawseti(L, -2, i); /* index[i] = index_uvalue */
		}
		lua_rawset(L, -3); /* namespace.index = index */

		lua_pushliteral(L, "cardinality");
		lua_pushinteger(L, namespace[n].cardinality);
		lua_rawset(L, -3); /* namespace.cardinality = cardinality */

		lua_pushliteral(L, "n");
		lua_pushinteger(L, n);
		lua_rawset(L, -3); /* namespace.n = n */

		lua_rawseti(L, -2, n); /* namespace_registry[n] = namespace */
	}
	lua_setglobal(L, "namespace_registry");
}
