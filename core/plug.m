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
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>

#include <third_party/queue.h>

#include <say.h>
#include <plug.h>

static struct plugs plugs;

void
plug_init(void)
{
	plugs.count = 0;
	SLIST_INIT(&plugs.list);
}

void
plug_free(void)
{
	struct plug * p;

	SLIST_FOREACH(p, &plugs.list, next) {

		if (p->free)
			p->free();

		free(p->path);
	}
}

struct plug*
plug_attach(char * path)
{
	void * fd = dlopen(path, RTLD_NOW);

	if (fd == NULL) {

		say_error("failed to load plugin %s: %s", path, dlerror());
		return NULL;
	}

	plug_initf init = dlsym(fd, PLUG_INIT);

	if (init == NULL) {

		dlclose(fd);
		say_error("failed to load plugin %s: '%s' symbol not found",
			path, PLUG_INIT);
		return NULL;
	}

	plug_freef ff = dlsym(fd, PLUG_FREE);

	struct plug_desc * desc = NULL;

	if (init(&desc) == TNT_MODULE_FAIL) {

		dlclose(fd);
		say_error("failed to init plugin %s", path);
		return NULL;
	}

	struct plug * p = malloc(sizeof(struct plug));

	if (p == NULL) {

		dlclose(fd);
		return NULL;
	}

	memset(p, 0, sizeof(struct plug));

	p->name = desc->name;
	p->desc = desc->desc;
	p->author = desc->author;
	p->path = strdup(path);
	p->free = ff;

	SLIST_INSERT_HEAD(&plugs.list, p, next);
	plugs.count++;

	return p;
}

static bool
plug_isext(char * path, char * ext) {

	char * ptr = strrchr(path, '.');

	if (ptr == NULL)
		return false;

	return (strcmp(ptr, ext) == 0);
}

void
plug_attach_dir(char * path)
{
	char fp[1024];
	struct dirent * de;
	DIR * dir = opendir(path);

	if (dir == NULL) {

		say_warn("failed to load plugin directory %s", path);
		return;
	}

	while ((de = readdir(dir))) {

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		if (!plug_isext(de->d_name, PLUG_EXT))
			continue;

		snprintf(fp, sizeof(fp), "%s/%s", path, de->d_name);

		plug_attach(fp);
	}

	closedir(dir);
}

void
plug_print(void)
{
	struct plug * p;

	say_info("%d plugins loaded", plugs.count);

	SLIST_FOREACH(p, &plugs.list, next)
		say_info("[plugin] %s: %s", p->name, p->desc);
}
