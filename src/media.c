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

#include "config.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include "media.h"
#include "tools.h"

struct media_pad *media_entity_remote_source(struct media_pad *pad)
{
	unsigned int i;

	if (!(pad->flags & MEDIA_PAD_FL_SINK))
		return NULL;

	for (i = 0; i < pad->entity->num_links; ++i) {
		struct media_link *link = &pad->entity->links[i];

		if (!(link->flags & MEDIA_LNK_FL_ENABLED))
			continue;

		if (link->sink == pad)
			return link->source;
	}

	return NULL;
}

struct media_entity *media_get_entity_by_name(struct media_device *media,
					      const char *name, size_t length)
{
	unsigned int i;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		if (strncmp(entity->info.name, name, length) == 0)
			return entity;
	}

	return NULL;
}

struct media_entity *media_get_entity_by_id(struct media_device *media,
					    __u32 id)
{
	unsigned int i;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		if (entity->info.id == id)
			return entity;
	}

	return NULL;
}

int media_setup_link(struct media_device *media,
		     struct media_pad *source,
		     struct media_pad *sink,
		     __u32 flags)
{
	struct media_link *link;
	struct media_link_desc ulink;
	unsigned int i;
	int ret;

	for (i = 0; i < source->entity->num_links; i++) {
		link = &source->entity->links[i];

		if (link->source->entity == source->entity &&
		    link->source->index == source->index &&
		    link->sink->entity == sink->entity &&
		    link->sink->index == sink->index)
			break;
	}

	if (i == source->entity->num_links) {
		printf("%s: Link not found\n", __func__);
		return -EINVAL;
	}

	/* source pad */
	ulink.source.entity = source->entity->info.id;
	ulink.source.index = source->index;
	ulink.source.flags = MEDIA_PAD_FL_SOURCE;

	/* sink pad */
	ulink.sink.entity = sink->entity->info.id;
	ulink.sink.index = sink->index;
	ulink.sink.flags = MEDIA_PAD_FL_SINK;

	ulink.flags = flags | (link->flags & MEDIA_LNK_FL_IMMUTABLE);

	ret = ioctl(media->fd, MEDIA_IOC_SETUP_LINK, &ulink);
	if (ret < 0) {
		printf("%s: Unable to setup link (%s)\n", __func__,
			strerror(errno));
		return ret;
	}

	link->flags = ulink.flags;
	link->twin->flags = ulink.flags;
	return 0;
}

int media_reset_links(struct media_device *media)
{
	unsigned int i, j;
	int ret;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		for (j = 0; j < entity->num_links; j++) {
			struct media_link *link = &entity->links[j];

			if (link->flags & MEDIA_LNK_FL_IMMUTABLE ||
			    link->source->entity != entity)
				continue;

			ret = media_setup_link(media, link->source, link->sink,
					       link->flags & ~MEDIA_LNK_FL_ENABLED);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static struct media_link *media_entity_add_link(struct media_entity *entity)
{
	if (entity->num_links >= entity->max_links) {
		struct media_link *links = entity->links;
		unsigned int max_links = entity->max_links * 2;
		unsigned int i;

		links = realloc(links, max_links * sizeof *links);
		if (links == NULL)
			return NULL;

		for (i = 0; i < entity->num_links; ++i)
			links[i].twin->twin = &links[i];

		entity->max_links = max_links;
		entity->links = links;
	}

	return &entity->links[entity->num_links++];
}

static int media_enum_links(struct media_device *media)
{
	__u32 id;
	int ret = 0;

	for (id = 1; id <= media->entities_count; id++) {
		struct media_entity *entity = &media->entities[id - 1];
		struct media_links_enum links;
		unsigned int i;

		links.entity = entity->info.id;
		links.pads = malloc(entity->info.pads * sizeof(struct media_pad_desc));
		links.links = malloc(entity->info.links * sizeof(struct media_link_desc));

		if (ioctl(media->fd, MEDIA_IOC_ENUM_LINKS, &links) < 0) {
			printf("%s: Unable to enumerate pads and links (%s).\n",
				__func__, strerror(errno));
			free(links.pads);
			free(links.links);
			return -errno;
		}

		for (i = 0; i < entity->info.pads; ++i) {
			entity->pads[i].entity = entity;
			entity->pads[i].index = links.pads[i].index;
			entity->pads[i].flags = links.pads[i].flags;
		}

		for (i = 0; i < entity->info.links; ++i) {
			struct media_link_desc *link = &links.links[i];
			struct media_link *fwdlink;
			struct media_link *backlink;
			struct media_entity *source;
			struct media_entity *sink;

			source = media_get_entity_by_id(media, link->source.entity);
			sink = media_get_entity_by_id(media, link->sink.entity);

			if (source == NULL || sink == NULL) {
				printf("WARNING entity %u link %u from %u/%u to %u/%u is invalid!\n",
					id, i, link->source.entity, link->source.index,
					link->sink.entity, link->sink.index);
				ret = -EINVAL;
			} else {
				fwdlink = media_entity_add_link(source);
				fwdlink->source = &source->pads[link->source.index];
				fwdlink->sink = &sink->pads[link->sink.index];
				fwdlink->flags = link->flags;

				backlink = media_entity_add_link(sink);
				backlink->source = &source->pads[link->source.index];
				backlink->sink = &sink->pads[link->sink.index];
				backlink->flags = link->flags;

				fwdlink->twin = backlink;
				backlink->twin = fwdlink;
			}
		}

		free(links.pads);
		free(links.links);
	}

	return ret;
}

#ifdef HAVE_LIBUDEV

#include <libudev.h>

static inline int media_udev_open(struct udev **udev)
{
	*udev = udev_new();
	if (*udev == NULL)
		return -ENOMEM;
	return 0;
}

static inline void media_udev_close(struct udev *udev)
{
	if (udev != NULL)
		udev_unref(udev);
}

static int media_get_devname_udev(struct udev *udev,
		struct media_entity *entity, int verbose)
{
	struct udev_device *device;
	dev_t devnum;
	const char *p;
	int ret = -ENODEV;

	if (udev == NULL)
		return -EINVAL;

	devnum = makedev(entity->info.v4l.major, entity->info.v4l.minor);
	if (verbose)
		printf("looking up device: %u:%u\n", major(devnum), minor(devnum));
	device = udev_device_new_from_devnum(udev, 'c', devnum);
	if (device) {
		p = udev_device_get_devnode(device);
		if (p) {
			strncpy(entity->devname, p, sizeof(entity->devname));
			entity->devname[sizeof(entity->devname) - 1] = '\0';
		}
		ret = 0;
	}

	udev_device_unref(device);

	return ret;
}

#else	/* HAVE_LIBUDEV */

struct udev;

static inline int media_udev_open(struct udev **udev) { return 0; }

static inline void media_udev_close(struct udev *udev) { }

static inline int media_get_devname_udev(struct udev *udev,
		struct media_entity *entity, int verbose)
{
	return -ENOTSUP;
}

#endif	/* HAVE_LIBUDEV */

static int media_get_devname_sysfs(struct media_entity *entity)
{
	struct stat devstat;
	char devname[32];
	char sysname[32];
	char target[1024];
	char *p;
	int ret;

	sprintf(sysname, "/sys/dev/char/%u:%u", entity->info.v4l.major,
		entity->info.v4l.minor);
	ret = readlink(sysname, target, sizeof(target));
	if (ret < 0)
		return -errno;

	target[ret] = '\0';
	p = strrchr(target, '/');
	if (p == NULL)
		return -EINVAL;

	sprintf(devname, "/dev/%s", p + 1);
	ret = stat(devname, &devstat);
	if (ret < 0)
		return -errno;

	/* Sanity check: udev might have reordered the device nodes.
	 * Make sure the major/minor match. We should really use
	 * libudev.
	 */
	if (major(devstat.st_rdev) == entity->info.v4l.major &&
	    minor(devstat.st_rdev) == entity->info.v4l.minor)
		strcpy(entity->devname, devname);

	return 0;
}

static int media_enum_entities(struct media_device *media, int verbose)
{
	struct media_entity *entity;
	struct udev *udev;
	unsigned int size;
	__u32 id;
	int ret;

	ret = media_udev_open(&udev);
	if (ret < 0)
		printf("%s: Can't get udev context\n", __func__);

	for (id = 0, ret = 0; ; id = entity->info.id) {
		size = (media->entities_count + 1) * sizeof(*media->entities);
		media->entities = realloc(media->entities, size);

		entity = &media->entities[media->entities_count];
		memset(entity, 0, sizeof(*entity));
		entity->fd = -1;
		entity->info.id = id | MEDIA_ENT_ID_FLAG_NEXT;

		ret = ioctl(media->fd, MEDIA_IOC_ENUM_ENTITIES, &entity->info);
		if (ret < 0) {
			ret = errno != EINVAL ? -errno : 0;
			break;
		}

		/* Number of links (for outbound links) plus number of pads (for
		 * inbound links) is a good safe initial estimate of the total
		 * number of links.
		 */
		entity->max_links = entity->info.pads + entity->info.links;

		entity->pads = malloc(entity->info.pads * sizeof(*entity->pads));
		entity->links = malloc(entity->max_links * sizeof(*entity->links));
		if (entity->pads == NULL || entity->links == NULL) {
			ret = -ENOMEM;
			break;
		}

		media->entities_count++;

		/* Find the corresponding device name. */
		if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE &&
		    media_entity_type(entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			continue;

		/* Try to get the device name via udev */
		if (!media_get_devname_udev(udev, entity, verbose))
			continue;

		/* Fall back to get the device name via sysfs */
		media_get_devname_sysfs(entity);
	}

	media_udev_close(udev);
	return ret;
}

struct media_device *media_open(const char *name, int verbose)
{
	struct media_device *media;
	int ret;

	media = calloc(1, sizeof(*media));
	if (media == NULL) {
		printf("%s: unable to allocate memory\n", __func__);
		return NULL;
	}

	if (verbose)
		printf("Opening media device %s\n", name);
	media->fd = open(name, O_RDWR);
	if (media->fd < 0) {
		media_close(media);
		printf("%s: Can't open media device %s\n", __func__, name);
		return NULL;
	}

	if (verbose)
		printf("Enumerating entities\n");

	ret = media_enum_entities(media, verbose);

	if (ret < 0) {
		printf("%s: Unable to enumerate entities for device %s (%s)\n",
			__func__, name, strerror(-ret));
		media_close(media);
		return NULL;
	}

	if (verbose) {
		printf("Found %u entities\n", media->entities_count);
		printf("Enumerating pads and links\n");
	}

	ret = media_enum_links(media);
	if (ret < 0) {
		printf("%s: Unable to enumerate pads and linksfor device %s\n",
			__func__, name);
		media_close(media);
		return NULL;
	}

	return media;
}

void media_close(struct media_device *media)
{
	unsigned int i;

	if (media->fd != -1)
		close(media->fd);

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		free(entity->pads);
		free(entity->links);
		if (entity->fd != -1)
			close(entity->fd);
	}

	free(media->entities);
	free(media);
}

