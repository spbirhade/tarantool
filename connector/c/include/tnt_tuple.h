#ifndef TNT_TUPLE_H_
#define TNT_TUPLE_H_

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

typedef struct _tnt_tuple_field_t {

	char * data;

	unsigned long size;
	unsigned long size_leb;

	struct _tnt_tuple_field_t * next;

} tnt_tuple_field_t;

typedef struct _tnt_tuple_t {

	unsigned long count;
	unsigned long size_enc;

	tnt_tuple_field_t * head;
	tnt_tuple_field_t * tail;

	struct _tnt_tuple_t * next;

} tnt_tuple_t;

#define TNT_TUPLE_FOREACH(TS, TI) \
	for ( (TI) = (TS)->head ; \
	      (TI)              ; \
	      (TI) = (TI)->next )

typedef struct {

	int count;

	tnt_tuple_t * head;
	tnt_tuple_t * tail;

} tnt_tuples_t;

#define TNT_TUPLES_FOREACH(TS, TI) \
	for ( (TI) = (TS)->head ; \
	      (TI)              ; \
	      (TI) = (TI)->next )

void
tnt_tuples_init(tnt_tuples_t * tuples);

void
tnt_tuples_free(tnt_tuples_t * tuples);

tnt_tuple_t*
tnt_tuples_add(tnt_tuples_t * tuples);

tnt_result_t
tnt_tuples_pack(tnt_tuples_t * tuples, char ** data, int * size);

tnt_result_t
tnt_tuples_unpack(tnt_tuples_t * tuples, char * data, int size);

void
tnt_tuple_init(tnt_tuple_t * tuple);

void
tnt_tuple_free(tnt_tuple_t * tuple);

tnt_result_t
tnt_tuple_add(tnt_tuple_t * tuple, char * data, int size);

tnt_result_t
tnt_tuple_pack(tnt_tuple_t * tuple, char ** data, int * size);

tnt_result_t
tnt_tuple_pack_to(tnt_tuple_t * tuple, char * dest);

#endif
