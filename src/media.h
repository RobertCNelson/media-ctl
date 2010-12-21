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

/**
 * @brief Open a media device.
 * @param name - name (including path) of the device node.
 * @param verbose - whether to print verbose information on the standard output.
 *
 * Open the media device referenced by @a name and enumerate entities, pads and
 * links.
 *
 * @return A pointer to a newly allocated media_device structure instance on
 * success and NULL on failure. The returned pointer must be freed with
 * media_close when the device isn't needed anymore.
 */
struct media_device *media_open(const char *name, int verbose);

/**
 * @brief Close a media device.
 * @param media - device instance.
 *
 * Close the @a media device instance and free allocated resources. Access to the
 * device instance is forbidden after this function returns.
 */
void media_close(struct media_device *media);

/**
 * @brief Locate the pad at the other end of a link.
 * @param pad - sink pad at one end of the link.
 *
 * Locate the source pad connected to @a pad through an enabled link. As only one
 * link connected to a sink pad can be enabled at a time, the connected source
 * pad is guaranteed to be unique.
 *
 * @return A pointer to the connected source pad, or NULL if all links connected
 * to @a pad are disabled. Return NULL also if @a pad is not a sink pad.
 */
struct media_pad *media_entity_remote_source(struct media_pad *pad);

/**
 * @brief Get the type of an entity.
 * @param entity - the entity.
 *
 * @return The type of @a entity.
 */
static inline unsigned int media_entity_type(struct media_entity *entity)
{
	return entity->info.type & MEDIA_ENT_TYPE_MASK;
}

/**
 * @brief Find an entity by its name.
 * @param media - media device.
 * @param name - entity name.
 * @param length - size of @a name.
 *
 * Search for an entity with a name equal to @a name.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
struct media_entity *media_get_entity_by_name(struct media_device *media,
	const char *name, size_t length);

/**
 * @brief Find an entity by its ID.
 * @param media - media device.
 * @param id - entity ID.
 *
 * Search for an entity with an ID equal to @a id.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
struct media_entity *media_get_entity_by_id(struct media_device *media,
	__u32 id);

/**
 * @brief Configure a link.
 * @param media - media device.
 * @param source - source pad at the link origin.
 * @param sink - sink pad at the link target.
 * @param flags - configuration flags.
 *
 * Locate the link between @a source and @a sink, and configure it by applying
 * the new @a flags.
 *
 * Only the MEDIA_LINK_FLAG_ENABLED flag is writable.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_setup_link(struct media_device *media,
	struct media_pad *source, struct media_pad *sink,
	__u32 flags);

/**
 * @brief Reset all links to the disabled state.
 * @param media - media device.
 *
 * Disable all links in the media device. This function is usually used after
 * opening a media device to reset all links to a known state.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_reset_links(struct media_device *media);

#endif

