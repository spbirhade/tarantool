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

#ifndef TARANTOOL_SILVERBOX_H
#define TARANTOOL_SILVERBOX_H

#include <mod/silverbox/assoc.h>

extern bool box_updates_allowed;
void memcached_handler(void *_data __unused__);

struct namespace;
struct box_tuple;

struct field {
	u32 len;
	union {
		u32 u32;

		u8 data[sizeof(void *)];

		void *data_ptr;
	};
};

enum field_data_type { NUM, STR };

struct tree_index_member {
	struct box_tuple *tuple;
	struct field key[];
};

#define SIZEOF_TREE_INDEX_MEMBER(index) \
	(sizeof(struct tree_index_member) + sizeof(struct field) * (index)->key_cardinality)

#include <third_party/sptree.h>
SPTREE_DEF(str_t, realloc);

// #include <mod/silverbox/tree.h>

struct index {
	bool enabled;

	struct box_tuple *(*find) (struct index * index, void *key);	/* only for unique lookups */
	struct box_tuple *(*find_by_tuple) (struct index * index, struct box_tuple * pattern);
	void (*remove) (struct index * index, struct box_tuple *);
	void (*replace) (struct index * index, struct box_tuple *, struct box_tuple *);
	void (*iterator_init) (struct index *, struct tree_index_member * pattern);
	struct box_tuple *(*iterator_next) (struct index *, struct tree_index_member * pattern);
	union {
		khash_t(lstr2ptr_map) * str_hash;
		khash_t(int2ptr_map) * int_hash;
		khash_t(int2ptr_map) * hash;
		sptree_str_t *tree;
	} idx;
	void *iterator;
	bool iterator_empty;

	struct namespace *namespace;

	struct {
		struct {
			u32 fieldno;
			enum field_data_type type;
		} *key_field;
		u32 key_cardinality;

		u32 *field_cmp_order;
		u32 field_cmp_order_cnt;
	};

	struct tree_index_member *search_pattern;

	enum { HASH, TREE } type;
};

extern struct index *memcached_index;

#define MAX_IDX 10
struct namespace {
	int n;
	bool enabled;
	int cardinality;
	struct index index[MAX_IDX];
};

struct box_tuple {
	u16 refs;
	u16 flags;
	u32 bsize;
	u32 cardinality;
	u8 data[0];
} __packed__;

#define MAX_SELECT_FIELDS 10
struct box_txn {
	int op;
	u32 flags;

	struct namespace *namespace;
	struct index *index;
	int n;

	struct tbuf *ref_tuples;
	struct box_tuple *old_tuple;
	struct box_tuple *tuple;
	struct box_tuple *lock_tuple;

	bool in_recover, old_format;

	struct {
		u32 fieldno;
		u32 offset;
		u32 len;
	} select_fmt[MAX_SELECT_FIELDS + 1];
};

enum tuple_flags {
	WAL_WAIT = 0x1,
	GHOST = 0x2,
	NEW = 0x4,
	SEARCH = 0x8
};

enum box_mode {
	RO = 1,
	RW
};

#define BOX_RETURN_TUPLE 1
#define BOX_ADD 2
#define BOX_REPLACE 4
#define BOX_QUIET 8

/*
    deprecated commands:
        _(INSERT, 1)
        _(DELETE, 2)
        _(SET_FIELD, 3)
        _(ARITH, 5)
        _(SET_FIELD, 6)
        _(ARITH, 7)
        _(SELECT, 4)
        _(DELETE, 8)
        _(UPDATE_FIELDS, 9)
        _(INSERT,10)
        _(SELECT_LIMIT, 12)
        _(SELECT_OLD, 14)
        _(UPDATE_FIELDS_OLD, 16)
        _(JUBOX_ALIVE, 11)

    DO NOT use those ids!
 */
#define MESSAGES(_)				\
        _(INSERT, 13)				\
        _(SELECT_LIMIT, 15)			\
	_(SELECT, 17)				\
	_(UPDATE_FIELDS, 19)			\
	_(DELETE, 20)				\
	_(SELECT_FIELDS, 21)

ENUM(messages, MESSAGES);

struct box_tuple *index_find(struct index *index, void *key);

struct box_txn *txn_alloc(u32 flags);
u32 box_dispach(struct box_txn *txn, enum box_mode mode, u32 op, struct tbuf *data);
void tuple_txn_ref(struct box_txn *txn, struct box_tuple *tuple);
void txn_cleanup(struct box_txn *txn);

void *next_field(void *f);
void append_field(struct tbuf *b, void *f);
void *tuple_field(struct box_tuple *tuple, size_t i);

void memcached_expire(void *data __unused__);
#endif
