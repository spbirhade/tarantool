
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

#include <say.h>
#include <user.h>

struct users users;

void
user_init(void)
{
	users.count = 0;
	SLIST_INIT(&users.list);
}

void
user_free(void)
{
	struct user * u;

	SLIST_FOREACH(u, &users.list, next) {
		free(u->id);
		free(u->key);
	}
}

struct user*
user_add(char * id, unsigned char * key, int key_size)
{
	struct user * u = malloc(sizeof(struct user));

	if (u == NULL)
		return NULL;

	u->id = strdup(id);

	if (u->id == NULL)
		return NULL;

	u->key_size = key_size;
	u->key = malloc(u->key_size + 1);

	if (u->key == NULL) {

		free(u->id);
		return NULL;
	}

	memcpy(u->key, key, u->key_size);
	u->key[u->key_size] = 0;

	SLIST_INSERT_HEAD(&users.list, u, next);
	users.count++;

	return u;
}

bool
user_del(char * id)
{
	struct user * u;

	SLIST_FOREACH(u, &users.list, next) {

		if (strcmp(u->id, id))
			continue;

		free(u->id);
		free(u->key);

		SLIST_REMOVE(&users.list, u, user, next);
		users.count--;
		return true;
	}

	return false;
}

struct user*
user_match(char * id)
{
	struct user * u;

	SLIST_FOREACH(u, &users.list, next)
		if (strcmp(u->id, id) == 0)
			return u;

	return NULL;
}

void
user_print(void)
{
	struct user * u;

	say_info("%d users", users.count);

	SLIST_FOREACH(u, &users.list, next)
		say_info("[user] %s", u->id);
}
