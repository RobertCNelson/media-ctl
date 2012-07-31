/*
 * Media controller interface library
 *
 * Copyright (C) 2010-2011 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
	struct media_device *media;
	struct media_entity_desc info;
	struct media_pad *pads;
	struct media_link *links;
	unsigned int max_links;
	unsigned int num_links;

	char devname[32];
	int fd;
	__u32 padding[6];
};

struct media_device;

/**
 * @brief Create a new media device.
 * @param devnode - device node path.
 *
 * Create a media device instance for the given device node and return it. The
 * device node is not accessed by this function, device node access errors will
 * not be caught and reported here. The media device needs to be enumerated
 * before it can be accessed, see media_device_enumerate().
 *
 * Media devices are reference-counted, see media_device_ref() and
 * media_device_unref() for more information.
 *
 * @return A pointer to the new media device or NULL if memory cannot be
 * allocated.
 */
struct media_device *media_device_new(const char *devnode);

/**
 * @brief Take a reference to the device.
 * @param media - device instance.
 *
 * Media devices are reference-counted. Taking a reference to a device prevents
 * it from being freed until all references are released. The reference count is
 * initialized to 1 when the device is created.
 *
 * @return A pointer to @a media.
 */
struct media_device *media_device_ref(struct media_device *media);

/**
 * @brief Release a reference to the device.
 * @param media - device instance.
 *
 * Release a reference to the media device. When the reference count reaches 0
 * this function frees the device.
 */
void media_device_unref(struct media_device *media);

/**
 * @brief Set a handler for debug messages.
 * @param media - device instance.
 * @param debug_handler - debug message handler
 * @param debug_priv - first argument to debug message handler
 *
 * Set a handler for debug messages that will be called whenever
 * debugging information is to be printed. The handler expects an
 * fprintf-like function.
 */
void media_debug_set_handler(
	struct media_device *media, void (*debug_handler)(void *, ...),
	void *debug_priv);

/**
 * @brief Enumerate the device topology
 * @param media - device instance.
 *
 * Enumerate the media device entities, pads and links. Calling this function is
 * mandatory before accessing the media device contents.
 *
 * @return Zero on success or a negative error code on failure.
 */
int media_device_enumerate(struct media_device *media);

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
 * This function searches for an entity based on its ID using an exact match or
 * next ID method based on the given @a id. If @a id is ORed with
 * MEDIA_ENT_ID_FLAG_NEXT, the function will return the entity with the smallest
 * ID larger than @a id. Otherwise it will return the entity with an ID equal to
 * @a id.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
struct media_entity *media_get_entity_by_id(struct media_device *media,
	__u32 id);

/**
 * @brief Get the number of entities
 * @param media - media device.
 *
 * This function returns the total number of entities in the media device. If
 * entities haven't been enumerated yet it will return 0.
 *
 * @return The number of entities in the media device
 */
unsigned int media_get_entities_count(struct media_device *media);

/**
 * @brief Get the entities
 * @param media - media device.
 *
 * This function returns a pointer to the array of entities for the media
 * device. If entities haven't been enumerated yet it will return NULL.
 *
 * The array of entities is owned by the media device object and will be freed
 * when the media object is destroyed.
 *
 * @return A pointer to an array of entities
 */
struct media_entity *media_get_entities(struct media_device *media);

/**
 * @brief Get the media device information
 * @param media - media device.
 *
 * The information structure is owned by the media device object and will be freed
 * when the media object is destroyed.
 *
 * @return A pointer to the media device information
 */
const struct media_device_info *media_get_info(struct media_device *media);

/**
 * @brief Get the media device node name
 * @param media - media device.
 *
 * The device node name string is owned by the media device object and will be
 * freed when the media object is destroyed.
 *
 * @return A pointer to the media device node name
 */
const char *media_get_devnode(struct media_device *media);

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
 * @return 0 on success, -1 on failure:
 *	   -ENOENT: link not found
 *	   - other error codes returned by MEDIA_IOC_SETUP_LINK
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

/**
 * @brief Parse string to a pad on the media device.
 * @param media - media device.
 * @param p - input string
 * @param endp - pointer to string where parsing ended
 *
 * Parse NULL terminated string describing a pad and return its struct
 * media_pad instance.
 *
 * @return Pointer to struct media_pad on success, NULL on failure.
 */
struct media_pad *media_parse_pad(struct media_device *media,
				  const char *p, char **endp);

/**
 * @brief Parse string to a link on the media device.
 * @param media - media device.
 * @param p - input string
 * @param endp - pointer to p where parsing ended
 *
 * Parse NULL terminated string p describing a link and return its struct
 * media_link instance.
 *
 * @return Pointer to struct media_link on success, NULL on failure.
 */
struct media_link *media_parse_link(struct media_device *media,
				    const char *p, char **endp);

/**
 * @brief Parse string to a link on the media device and set it up.
 * @param media - media device.
 * @param p - input string
 *
 * Parse NULL terminated string p describing a link and its configuration
 * and configure the link.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_parse_setup_link(struct media_device *media,
			   const char *p, char **endp);

/**
 * @brief Parse string to link(s) on the media device and set it up.
 * @param media - media device.
 * @param p - input string
 *
 * Parse NULL terminated string p describing link(s) separated by
 * commas (,) and configure the link(s).
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_parse_setup_links(struct media_device *media, const char *p);

#endif
