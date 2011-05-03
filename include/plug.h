#ifndef TARANTOOL_PLUG_H_INCLUDED
#define TARANTOOL_PLUG_H_INCLUDED

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

#define PLUG_INIT ("tnt_module_init")
#define PLUG_FREE ("tnt_module_free")
#define PLUG_EXT  (".so")

struct plug_desc {

	char * name;
	char * author;
	char * desc;
};

enum plug_result {

	TNT_MODULE_FAIL,
	TNT_MODULE_OK
};

typedef int  (*plug_initf)(struct plug_desc ** pd);
typedef void (*plug_freef)(void);

struct plug {
	
	void * fd;

	char * name;
	char * desc;
	char * author;
	char * path;

	plug_freef free;
	SLIST_ENTRY(plug) next;
};

struct plugs {

	int count;
	SLIST_HEAD(, plug) list;
};

void
plug_init(void);

void
plug_free(void);

struct plug*
plug_attach(char * path);

void
plug_attach_dir(char * path);

void
plug_print(void);

#endif
