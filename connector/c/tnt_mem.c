
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <tnt_error.h>
#include <tnt_mem.h>

static tnt_mallocf_t  _tnt_malloc  = (tnt_mallocf_t)malloc;
static tnt_reallocf_t _tnt_realloc = (tnt_reallocf_t)realloc;
static tnt_freef_t    _tnt_free    = (tnt_freef_t)free;
static tnt_dupf_t     _tnt_dup     = (tnt_dupf_t)strdup;

void
tnt_mem_init(tnt_mallocf_t m, tnt_reallocf_t r, tnt_dupf_t d, tnt_freef_t f)
{
	_tnt_malloc  = m;
	_tnt_realloc = r;
	_tnt_dup     = d;
	_tnt_free    = f;
}

void*
tnt_mem_alloc(int size)
{
	return _tnt_malloc(size);
}

void*
tnt_mem_realloc(void * ptr, int size)
{
	return _tnt_realloc(ptr, size);
}

char*
tnt_mem_dup(char * sz)
{
	return _tnt_dup(sz);
}

void
tnt_mem_free(void * ptr)
{
	_tnt_free(ptr);
}
