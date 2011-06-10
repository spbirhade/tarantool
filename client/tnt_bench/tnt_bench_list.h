#ifndef TNT_BENCH_LIST_H_
#define TNT_BENCH_LIST_H_

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

typedef struct _tnt_bench_list_node_t tnt_bench_list_node_t;
typedef struct _tnt_bench_list_t      tnt_bench_list_t;

struct _tnt_bench_list_node_t {
	int idx;
	int size;
	void * data;
	tnt_bench_list_node_t * next;
	tnt_bench_list_node_t * prev;
};

struct _tnt_bench_list_t {
	int id, count;
	int size;
	int alloc;
	tnt_bench_list_node_t * head;
	tnt_bench_list_node_t * tail;
};

#define TNT_BENCH_LIST_IDX(NODE) \
	((NODE)->idx)

#define TNT_BENCH_LIST_VALUE(NODE, TYPE) \
	((TYPE)((NODE)->data))

#define TNT_BENCH_LIST_COUNT(LIST) \
	((LIST)->count)

#define TNT_BENCH_LIST_ID(LIST) \
	((LIST)->id)

#define TNT_BENCH_LIST_SIZE(LIST) \
	((LIST)->size)

#define TNT_BENCH_LIST_FOREACH(LIST, ITER) \
	for ( (ITER) = (LIST)->head ; \
	      (ITER)                ; \
	      (ITER) = (ITER)->next )

#define TNT_BENCH_LIST_FOREACH_BACKWARD(LIST, ITER) \
	for ( (ITER) = (LIST)->tail ; \
	      (ITER)                ; \
	      (ITER) = (ITER)->prev )

void
tnt_bench_list_init(tnt_bench_list_t * list, int alloc);

void
tnt_bench_list_free(tnt_bench_list_t * list);

void*
tnt_bench_list_add(tnt_bench_list_t * list, void * data, int size);

void*
tnt_bench_list_alloc(tnt_bench_list_t * list, int size);

void
tnt_bench_list_del_node(tnt_bench_list_t * list, tnt_bench_list_node_t * node);

int
tnt_bench_list_del(tnt_bench_list_t * list, void * data);

void
tnt_bench_list_del_last(tnt_bench_list_t * list);

#endif
