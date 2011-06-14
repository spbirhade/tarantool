
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

#include <client/tnt_bench/tnt_bench_list.h>

void
tnt_bench_list_init(tnt_bench_list_t * list, int alloc)
{
	list->alloc = alloc;
	list->id    = 0;
	list->count = 0;
	list->size  = 0;
	list->head  = NULL;
	list->tail  = NULL;
}

void
tnt_bench_list_free(tnt_bench_list_t * list)
{
	tnt_bench_list_node_t * next;
	tnt_bench_list_node_t * current;
	for (current = list->head ; current ; current = next) {
		next = current->next;
		if (list->alloc)
			free(current->data);
		free(current);
	}
	tnt_bench_list_init(list, list->alloc);
}

void*
tnt_bench_list_add(tnt_bench_list_t * list, void * data, int size)
{
	tnt_bench_list_node_t * new =
			malloc(sizeof(tnt_bench_list_node_t));
	if (new == NULL)
		return NULL;

	new->idx = list->count;
	if (list->alloc) {
		new->data = malloc(size);
		if (new->data == NULL) {
			free(new);
			return NULL;
		}
		if (data)
			memcpy(new->data, data, size);
	} else
		new->data = data;

	new->size = size;
	new->prev = list->tail;
	new->next = NULL;
	if (list->head == NULL)
		list->head = new;
	else
		list->tail->next = new;

	list->tail = new;
	list->size = list->size + size;
	list->count++;
	list->id++;
	return new->data;
}

void*
tnt_bench_list_alloc(tnt_bench_list_t * list, int size)
{
	return tnt_bench_list_add(list, NULL, size);
}

void
tnt_bench_list_del_node(tnt_bench_list_t * list, tnt_bench_list_node_t * node)
{
	if (node == list->head) {
		if (list->head == list->tail)
			list->tail = node->next;
		list->head = node->next;
		if (node->next)
			node->next->prev = NULL;
	} else {
		if (node == list->tail)
			list->tail = node->prev;
		node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;
	}

	list->size = list->size - node->size;
	list->count--;

	if (list->alloc)
		free(node->data);
	free(node);
}

int
tnt_bench_list_del(tnt_bench_list_t * list, void * data)
{
	tnt_bench_list_node_t * current;
	TNT_BENCH_LIST_FOREACH(list, current) {
		if (current->data == data) {
			tnt_bench_list_del_node(list, current);
			return 0;
		}
	}
	return -1;
}

void
tnt_bench_list_del_last(tnt_bench_list_t * list)
{
	if (TNT_BENCH_LIST_COUNT(list) == 0)
		return;
	tnt_bench_list_del_node(list, list->tail);
}
