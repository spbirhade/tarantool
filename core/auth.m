
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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <third_party/queue.h>

#include <fiber.h>
#include <say.h>
#include <iproto.h>
#include <admin.h>
#include <auth.h>

extern struct auth_mech * auth_mech;
static struct auth auth;

void
auth_init(void)
{
	auth.id = 0;
	auth.count = 0;
	SLIST_INIT(&auth.list);
}

void
auth_free(void)
{
	struct auth_mech * a;

	SLIST_FOREACH(a, &auth.list, next) {

		if (a->free)
			a->free();

		free(a->name);
		free(a->desc);
	}
}

int
auth_add(char * name, char * desc,
	auth_initf init, auth_freef ff, auth_handshakef hs)
{
	struct auth_mech * am = malloc(sizeof(struct auth_mech));

	if (am == NULL)
		return -1;

	if (init && (init() == false)) {

		free(am);
		return -1;
	}

	am->id   = auth.id++;
	am->name = strdup(name);
	am->desc = strdup(desc);

	am->free = ff;
	am->handshake = hs;

	SLIST_INSERT_HEAD(&auth.list, am, next);
	auth.count++;

	return am->id;
}

bool
auth_del(int id)
{
	struct auth_mech * a;

	SLIST_FOREACH(a, &auth.list, next) {

		if (a->id != id)
			continue;

		if (a->free)
			a->free();

		free(a->name);
		free(a->desc);

		SLIST_REMOVE(&auth.list, a, auth_mech, next);
		auth.count--;

		return true;
	}

	return false;
}

struct auth_mech*
auth_match_id(int id)
{
	struct auth_mech * am;

	SLIST_FOREACH(am, &auth.list, next)
		if (am->id == id)
			return am;
	return NULL;
}

struct auth_mech*
auth_match_name(char * name)
{
	struct auth_mech * am;

	SLIST_FOREACH(am, &auth.list, next)
		if (!strcmp(am->name, name))
			return am;
	return NULL;
}

void
auth_print(void)
{
	struct auth_mech * am;

	say_info("%d authentication mechanisms loaded", auth.count);

	SLIST_FOREACH(am, &auth.list, next)
		say_info("[auth] %s: %s", am->name, am->desc);
}
