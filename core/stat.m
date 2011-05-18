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
#include "stat.h"

#include <util.h>
#include <tarantool_ev.h>
#include <tbuf.h>
#include <say.h>
#include <fiber.h>

#include <third_party/khash.h>

#define SECS 5

struct {
	char *name;
	i64 value;
} *stats = NULL;
static int stats_size = 0;
static int stats_max = 0;
static int base = 0;

int
stat_register(char **name, size_t max_idx)
{
	int initial_base = base;

	for (int i = 0; i < max_idx; i++, name++, base++) {
		if (stats_size <= base) {
			stats_size += 1024;
			stats = realloc(stats, sizeof(*stats) * stats_size);
			if (stats == NULL)
				abort();
		}

		stats[base].name = *name;

		if (*name == NULL)
			continue;

		stats[base].value = 0;

		stats_max = base;
	}

	return initial_base;
}

void
stat_collect(int base, int name, i64 value)
{
	stats[base + name].value += value;
}

void
stat_print(lua_State *L, struct tbuf *buf)
{
	lua_getglobal(L, "stat");
	lua_getfield(L, -1, "print");
	luaT_pushtbuf(L, buf);
	if (lua_pcall(L, 1, 0, 0) != 0) {
		say_error("lua_pcall(stat.print): %s", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static void
stat_record(void *data __attribute__((unused)))
{
	lua_State *L = fiber->L;

	for (;;) {
                fiber_sleep(1);

                if (stats == NULL)
                        continue;

		lua_getglobal(L, "stat");
		lua_getfield(L, -1, "record"); /* stack top is stat.record */

		lua_newtable(L); /* table with stats */
		for (int i = 0; i <= stats_max; i++) {
			if (stats[i].name == NULL)
				continue;

			lua_pushstring(L, stats[i].name);
			lua_pushnumber(L, stats[i].value);
			lua_settable(L, -3);
		}

		if (lua_pcall(L, 1, 0, 0) != 0) {
			say_error("lua_pcall(stat.record): %s", lua_tostring(L, -1));
			lua_pop(L, 1);
		}

		for (int i = 0; i <= stats_max; i++) {
				if (stats[i].name == NULL)
					continue;
				stats[i].value = 0;
		}
	}
}

void
stat_clear()
{
        lua_State *L = fiber->L;

        for (int i = 0; i <= stats_max; i++) {
                if (stats[i].name == NULL)
                        continue;
                stats[i].value = 0;
        }

        lua_getglobal(L, "stat");
        lua_getfield(L, -1, "clear");

        if (lua_pcall(L, 0, 0, 0) != 0) {
                say_error("lua_pcall(stat.clear): %s", lua_tostring(L, -1));
                lua_pop(L, 1);
        }
}

void
stat_init()
{
	struct fiber *s = fiber_create("stat", -1, -1, stat_record, NULL);
	fiber_call(s);
}
