
/*
 * Copyright (C) 2011 Mail.RU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitnted provided that the following conditions
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

#include <tnt_result.h>
#include <tnt_leb128.h>
#include <tnt_tuple.h>

void
tnt_tuples_init(tnt_tuples_t * tuples)
{
	tuples->count = 0;
	tuples->head  = NULL;
	tuples->tail  = NULL;
}

void
tnt_tuples_free(tnt_tuples_t * tuples)
{
	tnt_tuple_t * t, * next;

	for (t = tuples->head ; t ; t = next) {

		next = t->next;
		tnt_tuple_free(t);
	}
}

tnt_tuple_t*
tnt_tuples_add(tnt_tuples_t * tuples)
{
	tnt_tuple_t * t = malloc(sizeof(tnt_tuple_t));

	if (t == NULL)
		return NULL;

	if (tuples->head == NULL)
		tuples->head = t;
	else
		tuples->tail->next = t;

	tuples->tail = t;
	tuples->count++;

	tnt_tuple_init(t);
	return t;
}

tnt_result_t
tnt_tuples_pack(tnt_tuples_t * tuples, char ** data, int * size)
{
	tnt_tuple_t * t;

	if (tuples->count == 0)
		return TNT_ENOTU;

	*size = 4; /* count */

	for (t = tuples->head ; t ; t = t->next)
		*size += t->size_enc;

	*data = malloc(*size);

	if (*data == NULL)
		return TNT_EMEMORY;

	tnt_result_t result;
	char * p = *data;

	memcpy(p, &tuples->count, sizeof(unsigned long));
	p += 4;

	for (t = tuples->head ; t ; t = t->next) {

		result = tnt_tuple_pack_to(t, p);

		if (result != TNT_EOK) {

			free(*data);
			return result;
		}

		p += t->size_enc;
	}

	return TNT_EOK;
}

tnt_result_t
tnt_tuples_unpack(tnt_tuples_t * tuples, char * data, int size)
{
	tnt_tuple_t * t = tnt_tuples_add(tuples);

	if (t == NULL) {

		tnt_tuples_free(tuples);
		return TNT_EMEMORY;
	}

	char * p = data;
	int off;
	unsigned long i, c = *(unsigned long*)p;

	off = sizeof(unsigned long), p += sizeof(unsigned long);

	for (i = 0 ; i < c ; i++) {

		unsigned long s;
		int r = tnt_leb128_read(p, size - off, &s);

		if (r == -1) 
			return TNT_EPROTO;

		off += r, p += r;

		if (s > (unsigned long)(size - off))
			return TNT_EPROTO;

		tnt_result_t res = tnt_tuple_add(t, p, s);

		if ( res != TNT_EOK )
			return res;

		off += s, p+= s;
	}

	return TNT_EOK;
}

void
tnt_tuple_init(tnt_tuple_t * tuple)
{
	tuple->count    = 0;
	tuple->size_enc = 4; /* cardinality */

	tuple->head     = NULL;
	tuple->tail     = NULL;
	tuple->next     = NULL;
}

void
tnt_tuple_free(tnt_tuple_t * tuple)
{
	tnt_tuple_field_t * f, * next;

	for (f = tuple->head ; f ; f = next) {

		next = f->next;
		
		free(f->data);
		free(f);
	}
}

tnt_result_t
tnt_tuple_add(tnt_tuple_t * tuple, char * data, int size)
{
	tnt_tuple_field_t * f = malloc(sizeof(tnt_tuple_field_t));

	if (f == NULL)
		return TNT_EMEMORY;

	if (tuple->head == NULL)
		tuple->head = f;
	else
		tuple->tail->next = f;

	tuple->tail = f;
	tuple->count++;

	f->size = size;
	f->size_leb = 0;
	f->data = malloc(size);
	f->next = NULL;

	if (f->data == NULL) {

		free(f);
		return TNT_EMEMORY;
	}

	f->size_leb = tnt_leb128_size(size);

	tuple->size_enc += f->size_leb;
	tuple->size_enc += size;

	memcpy(f->data, data, f->size);
	return TNT_EOK;
}

tnt_result_t
tnt_tuple_pack(tnt_tuple_t * tuple, char ** data, int * size)
{
	char             * p;
	tnt_tuple_field_t * f;

	*size = tuple->size_enc;
	*data = malloc(tuple->size_enc);

	if (*data == NULL)
		return TNT_EMEMORY;

	p  = *data;

	memcpy(p, &tuple->count, sizeof(unsigned long));
	p += 4;

	for (f = tuple->head ; f ; f = f->next) {

		tnt_leb128_write(p, f->size);
		p += f->size_leb;

		memcpy(p, f->data, f->size); 
		p += f->size;
	}

	return TNT_EOK;
}

tnt_result_t
tnt_tuple_pack_to(tnt_tuple_t * tuple, char * dest)
{
	tnt_tuple_field_t * f;

	memcpy(dest, &tuple->count, sizeof(unsigned long));
	dest += 4;

	for (f = tuple->head ; f ; f = f->next) {

		tnt_leb128_write(dest, f->size);
		dest += f->size_leb;

		memcpy(dest, f->data, f->size); 
		dest += f->size;
	}

	return TNT_EOK;
}
