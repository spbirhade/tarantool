#ifndef TNT_PROTO_H_
#define TNT_PROTO_H_

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

/* doc/box-protocol.txt */

#define TNT_PROTO_TYPE_INSERT (13)
#define TNT_PROTO_TYPE_SELECT (17)
#define TNT_PROTO_TYPE_UPDATE (19)
#define TNT_PROTO_TYPE_DELETE (20)
#define TNT_PROTO_TYPE_PING   (65280)

typedef struct {
	unsigned long type;
	unsigned long len;
	unsigned long reqid;
} tnt_proto_header_t;

#define TNT_PROTO_IS_OK(V)    (((V) & 0x00) == 0x0)
#define TNT_PROTO_IS_AGAIN(V) (((V) & 0x01) == 0x1)
#define TNT_PROTO_IS_ERROR(V) (((V) & 0x02) == 0x2)

/* OK */
#define TNT_PROTO_ERR_CODE_OK                   (0x00000000)
/* Non master connection, but it should be */ 
#define TNT_PROTO_ERR_CODE_NONMASTER            (0x00000102)
/* Illegal parametrs */ 
#define TNT_PROTO_ERR_CODE_ILLEGAL_PARAMS       (0x00000202)
/* Uid not from this storage range */ 
#define TNT_PROTO_ERR_CODE_BAD_UID              (0x00000302)
/* Node is marked as read-only */ 
#define TNT_PROTO_ERR_CODE_NODE_IS_RO           (0x00000401)
/* Node isn't locked */ 
#define TNT_PROTO_ERR_CODE_NODE_IS_NOT_LOCKED   (0x00000501)
/* Node is locked */ 
#define TNT_PROTO_ERR_CODE_NODE_IS_LOCKED       (0x00000601)
/* Some memory issues */ 
#define TNT_PROTO_ERR_CODE_MEMORY_ISSUE         (0x00000701)
/* Bad graph integrity */ 
#define TNT_PROTO_ERR_CODE_BAD_INTEGRITY        (0x00000802)
/* Unsupported command */ 
#define TNT_PROTO_ERR_CODE_UNSUPPORTED_COMMAND  (0x00000a02)

/* Can't register new user */ 
#define TNT_PROTO_ERR_CODE_CANNOT_REGISTER      (0x00001801)
/* Can't generate alert id */ 
#define TNT_PROTO_ERR_CODE_CANNOT_INIT_ALERT_ID (0x00001a01)
/* Can't del node */ 
#define TNT_PROTO_ERR_CODE_CANNOT_DEL           (0x00001b02)
/* User isn't registered */ 
#define TNT_PROTO_ERR_CODE_USER_NOT_REGISTERED  (0x00001c02)

/* Syntax error in query */ 
#define TNT_PROTO_ERR_CODE_SYNTAX_ERROR         (0x00001d02)
/* Unknown field */ 
#define TNT_PROTO_ERR_CODE_WRONG_FIELD          (0x00001e02)
/* Number value is out of range */ 
#define TNT_PROTO_ERR_CODE_WRONG_NUMBER         (0x00001f02)
/* Insert already existing object */ 
#define TNT_PROTO_ERR_CODE_DUPLICATE            (0x00002002)
/* Can not order result */ 
#define TNT_PROTO_ERR_CODE_UNSUPPORTED_ORDER    (0x00002202)
/* Multiple to update/delete */ 
#define TNT_PROTO_ERR_CODE_MULTIWRITE           (0x00002302)
/* nothing to do (not an error) */ 
#define TNT_PROTO_ERR_CODE_NOTHING              (0x00002400)
/* id's update */ 
#define TNT_PROTO_ERR_CODE_UPDATE_ID            (0x00002502)
/* unsupported version of protocol */ 
#define TNT_PROTO_ERR_CODE_WRONG_VERSION        (0x00002602)

/* other generic error codes */
#define TNT_PROTO_ERR_CODE_UNKNOWN_ERROR        (0x00002702)
#define TNT_PROTO_ERR_CODE_NODE_NOT_FOUND       (0x00003102)
#define TNT_PROTO_ERR_CODE_NODE_FOUND           (0x00003702)
#define TNT_PROTO_ERR_CODE_INDEX_VIOLATION      (0x00003802)
#define TNT_PROTO_ERR_CODE_NO_SUCH_NAMESPACE    (0x00003902)

typedef struct {
	tnt_proto_header_t hdr;
	unsigned long code;
} tnt_proto_header_resp_t;

typedef struct {
	unsigned long card;
	unsigned char field[];
} tnt_proto_tuple_t;

#define TNT_PROTO_FLAG_RETURN    (0x01)
#define TNT_PROTO_FLAG_ADD       (0x02)
#define TNT_PROTO_FLAG_REPLACE   (0x04)
#define TNT_PROTO_FLAG_BOX_QUIET (0x08)
#define TNT_PROTO_FLAG_NOT_STORE (0x10)

typedef struct {
	unsigned long ns;
	unsigned long flags;
	/* tuple data */
} tnt_proto_insert_t;

#define TNT_PROTO_UPDATE_ASSIGN (0)
#define TNT_PROTO_UPDATE_ADD    (1)
#define TNT_PROTO_UPDATE_AND    (2)
#define TNT_PROTO_UPDATE_XOR    (3)
#define TNT_PROTO_UPDATE_OR     (4)

typedef struct {
	unsigned long ns;
	unsigned long flags;
	/* tuple data */
	/* count */
	/* operation */
} tnt_proto_update_t;

typedef struct {
	unsigned long field;
	unsigned char op;
	/* op_arg */
} tnt_proto_update_op_t;

typedef struct {
	unsigned long ns;
	/* tuple data */
} tnt_proto_delete_t;

typedef struct {
	unsigned long ns;
	unsigned long index;
	unsigned long offset;
	unsigned long limit;
	/* tuple data */
} tnt_proto_select_t;

#endif
