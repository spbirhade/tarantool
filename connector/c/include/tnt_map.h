#ifndef TNT_MAP_H_
#define TNT_MAP_H_

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

typedef enum {

	TNT_MAP_OFFLINE,
	TNT_MAP_ONLINE

} tnt_map_status_t;

typedef struct {

	int idx;
	tnt_map_status_t status;
	
	char * host;
	int port;
	int points;
	int mem;
	int reserved;

	unsigned long long stat_maps;

} tnt_map_server_t;

typedef struct {

	int idx;
	unsigned int point;

} tnt_map_ptr_t;

typedef struct {

	unsigned long points;
	int mem;

	tnt_map_ptr_t * map;

	int servers_count;
	int servers_max;
	int servers_per;
	int servers_online;

	tnt_map_server_t * servers;

} tnt_map_t;

tnt_result_t
tnt_map_init(tnt_map_t * map, int per);

void
tnt_map_free(tnt_map_t * map);

tnt_result_t
tnt_map_rehash(tnt_map_t * map);

int
tnt_map_add(tnt_map_t * map, char * host, int port, int mem);

void
tnt_map_online(tnt_map_t * map, int idx);

void
tnt_map_offline(tnt_map_t * map, int idx);

void
tnt_map_remap(tnt_map_t * map, int idx, char * host, int port);

tnt_map_server_t*
tnt_map(tnt_map_t * map, char * key, int size);

void
tnt_map_show(tnt_map_t * map);

#endif
