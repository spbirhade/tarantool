
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

/* based on libketama */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_md5.h>
#include <tnt_map.h>

int
tnt_map_init(tnt_map_t * map, int per)
{
	map->points = 0;
	map->mem = 0;
	map->map = NULL;
	map->servers_count  = 0;
	map->servers_max    = 2;
	map->servers_per    = per;
	map->servers_online = 0;

	map->servers = tnt_mem_alloc(sizeof(tnt_map_server_t) *
			map->servers_max);
	if (map->servers == NULL)
		return -1;
	memset(map->servers, 0,
		sizeof(tnt_map_server_t) * map->servers_max);
	return 0;
}

void
tnt_map_free(tnt_map_t * map)
{
	if (map->map)
		tnt_mem_free(map->map);

	int i;
	for (i = 0 ; i < map->servers_max ; i++) {
		if (map->servers[i].host)
			tnt_mem_free(map->servers[i].host);
	}

	if (map->servers)
		tnt_mem_free(map->servers);
}

int
tnt_map_add(tnt_map_t * map, char * host, int port, int mem)
{
	if (map->servers_count >= map->servers_max) {
		map->servers_max *= 2;
		tnt_map_server_t * ptr = tnt_mem_realloc(map->servers,
			sizeof(tnt_map_server_t) * map->servers_max);
		if (ptr == NULL)
			return -1;
		map->servers = ptr;
	}

	map->servers[map->servers_count].host = tnt_mem_dup(host);
	if (map->servers[map->servers_count].host == NULL)
		return -1;

	int idx = map->servers_count++;
	map->servers[idx].idx = idx;
	map->servers[idx].status = TNT_MAP_ONLINE;
	map->servers[idx].port = port;
	map->servers[idx].points = 0;
	map->servers[idx].mem = mem;
	map->servers[idx].reserved = 0;
	map->servers[idx].stat_maps = 0;

	map->mem += mem;
	map->servers_online++;
	return idx;
}

void
tnt_map_online(tnt_map_t * map, int idx)
{
	map->servers[idx].status = TNT_MAP_OFFLINE;
	map->servers_online++;
}

void
tnt_map_offline(tnt_map_t * map, int idx)
{
	map->servers[idx].status = TNT_MAP_OFFLINE;
	map->servers_online--;
}

void
tnt_map_remap(tnt_map_t * map, int idx, char * host, int port)
{
	tnt_mem_free(map->servers[idx].host);

	map->servers[idx].host = tnt_mem_dup(host);
	map->servers[idx].port = port;
}

static void
tnt_map_digest(unsigned char * digest, char * key, int size)
{
	tnt_md5_t md5;

    tnt_md5_init(&md5);
    tnt_md5_update(&md5, (unsigned char*)key, size);
    tnt_md5_final(digest, &md5);
}

int
tnt_map_cmpf(tnt_map_ptr_t * a, tnt_map_ptr_t * b)
{
	if (a->point < b->point)
		return -1;
	if (a->point > b->point)
		return 1;
	return 0;
}

int
tnt_map_rehash(tnt_map_t * map)
{
	tnt_map_ptr_t * ptr =
		tnt_mem_realloc(map->map, sizeof(tnt_map_ptr_t) *
			 map->servers_count *
			(map->servers_per * 4));
	if (ptr == NULL)
		return -1;

	map->map = ptr;
	int i, j, k, current = 0;
	for (i = 0 ; i < map->servers_count ; i++) {
		float mem = (float)map->servers[i].mem / (float)map->mem;
		map->servers[i].points   =
			floorf(mem * (float)map->servers_count * (float)map->servers_per);
		map->servers[i].reserved = floorf(mem * 100.0);
		for (j = 0 ; j < map->servers[i].points ; j++) {
			unsigned char digest[TNT_MD5_DIGEST_LENGTH];
			char sz[64];
			int len = snprintf(sz, sizeof(sz), "%s-%d-%d",
				map->servers[i].host,
				map->servers[i].port,
				j);

			tnt_map_digest(digest, sz, len);
			for (k = 0 ; k < 4 ; k++) {
				map->map[current].idx   = i;
				map->map[current].point =
					((digest[3 + k * 4] << 24) |
				     (digest[2 + k * 4] << 16) |
				     (digest[1 + k * 4] <<  8) |
				     (digest[    k * 4]));

				current++;
			}
		}
	}

	map->points = current;
	/* points must be sorted for searching */
    qsort((void*)map->map, current, sizeof(tnt_map_ptr_t),
		(int(*)(const void *, const void *))
			tnt_map_cmpf);
	return 0;
}

static long
tnt_map_hash(char * key, int size)
{
	unsigned char digest[TNT_MD5_DIGEST_LENGTH];
	tnt_map_digest(digest, key, size);
	return (unsigned long)((digest[3] << 24) |
	                       (digest[2] << 16) |
	                       (digest[1] <<  8) |
	                       (digest[0]));
}

#if 0
static tnt_map_server_t*
tnt_map_do(tnt_map_t * map, char * key, int size)
{
	if (map->servers_online == 0)
		return NULL;

	unsigned long hash; 
	unsigned long point_low, point_middle, point_high;
	unsigned long value, value_prev;

	point_low  = 0;
	point_high = map->points;

	hash = tnt_map_hash(key, size);

#define TNT_MAP_ISON(MAP, IDX) \
	((MAP)->servers[(MAP)->map[(IDX)].idx].status == TNT_MAP_ONLINE)

	/* using binary-search alike method to find
	 * server with biggest point after this key hashes to.
	 *
	 * points are sorted.
	 */

	while (1) {

		point_middle = (point_low + point_high) / 2;

		if (point_middle == map->points)
			break;

		value = map->map[point_middle].point;

		if (point_middle == 0)
			value_prev = 0;
		else
			value_prev = map->map[point_middle - 1].point;

		if ((hash <= value) && (hash > value_prev)) {

			if (TNT_MAP_ISON(map, point_middle))
				return &map->servers[map->map[point_middle].idx];
		}

		if (value < hash)
			point_low = point_middle + 1;
		else
			point_high = point_middle - 1;

		if (point_low > point_high)
			break;
	}

	unsigned int i;

	for (i = 0 ; i < map->points ; i++)
		if (TNT_MAP_ISON(map, i))
			return &map->servers[map->map[i].idx];

	return NULL;
}

tnt_map_server_t*
tnt_map(tnt_map_t * map, char * key, int size)
{
	tnt_map_server_t * s = tnt_map_do(map, key, size);

	s->stat_maps++;
	return s;
}
#endif

static tnt_map_server_t*
tnt_map_do(tnt_map_t * map, char * key, int size)
{
	unsigned long hash; 
	unsigned long point_low, point_middle, point_high;
	unsigned long value, value_prev;

	point_low  = 0;
	point_high = map->points;

	hash = tnt_map_hash(key, size);

	/* using binary-search alike method to find
	 * server with biggest point after this key hashes to.
	 *
	 * points are sorted.
	 */

	while (1) {
		point_middle = (point_low + point_high) / 2;
		if (point_middle == map->points)
			return &map->servers[map->map[0].idx];

		value = map->map[point_middle].point;
		if (point_middle == 0)
			value_prev = 0;
		else
			value_prev = map->map[point_middle - 1].point;

		if ((hash <= value) && (hash > value_prev))
			return &map->servers[map->map[point_middle].idx];

		if (value < hash)
			point_low = point_middle + 1;
		else
			point_high = point_middle - 1;

		if (point_low > point_high)
			return &map->servers[map->map[0].idx];
	}

	return NULL;
}

tnt_map_server_t*
tnt_map(tnt_map_t * map, char * key, int size)
{
	tnt_map_server_t * s = tnt_map_do(map, key, size);
	if (s->status == TNT_MAP_OFFLINE) {
		if (map->servers_online == 0)
			return NULL;
		int i;
		for (i = s->idx + 1 ;; i++) {
			if (i == map->servers_count) {
				i = -1;
				continue;
			}

			if (map->servers[i].status == TNT_MAP_ONLINE) {
				map->servers[i].stat_maps++;
				return &map->servers[i];
			}
		}
	}

	s->stat_maps++;
	return s;
}

void
tnt_map_show(tnt_map_t * map)
{
	if (map->map == NULL)
		return;

	int i;
	for (i = 0 ; i < map->servers_count ; i++) {
		char addr[64];
		snprintf(addr, sizeof(addr), "%s:%d", 
			map->servers[i].host,
			map->servers[i].port);

		printf("[%2d] %-14s %8s (mem: %3d / %3d) maps: %llu\n",
			i,
			addr,
			(map->servers[i].status == TNT_MAP_ONLINE) ? "online" : "offline",
			map->servers[i].points,
			map->servers_per * map->servers_count,
			map->servers[i].stat_maps);
	}
}
