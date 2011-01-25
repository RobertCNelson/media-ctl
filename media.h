/*
 * Media controller test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#ifndef __MEDIA_H__
#define __MEDIA_H__

#include <linux/media.h>

struct media_link {
	struct media_pad *source;
	struct media_pad *sink;
	struct media_link *twin;
	__u32 flags;
	__u32 padding[3];
};

struct media_pad {
	struct media_entity *entity;
	__u32 index;
	__u32 flags;
	__u32 padding[3];
};

struct media_entity {
	struct media_entity_desc info;
	struct media_pad *pads;
	struct media_link *links;
	unsigned int max_links;
	unsigned int num_links;

	char devname[32];
	int fd;
	__u32 padding[6];
};

struct media_device {
	int fd;
	struct media_entity *entities;
	unsigned int entities_count;
	__u32 padding[6];
};

struct media_device *media_open(const char *name, int verbose);
void media_close(struct media_device *media);

struct media_pad *media_entity_remote_source(struct media_pad *pad);

static inline unsigned int media_entity_type(struct media_entity *entity)
{
	return entity->info.type & MEDIA_ENTITY_TYPE_MASK;
}

struct media_entity *media_get_entity_by_name(struct media_device *media,
	const char *name, size_t length);
struct media_entity *media_get_entity_by_id(struct media_device *media,
	__u32 id);
int media_setup_link(struct media_device *media,
	struct media_pad *source, struct media_pad *sink,
	__u32 flags);
int media_reset_links(struct media_device *media);

#endif

