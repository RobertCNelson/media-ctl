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

static const char *media_entity_type_to_string(unsigned type)
{
	static const struct {
		__u32 type;
		const char *name;
	} types[] = {
		{ MEDIA_ENTITY_TYPE_NODE, "Node" },
		{ MEDIA_ENTITY_TYPE_SUBDEV, "V4L2 subdev" },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		if (types[i].type == type)
			return types[i].name;
	}

	return "Unknown";
}

static const char *media_entity_subtype_to_string(unsigned type, unsigned subtype)
{
	static const char *node_types[] = {
		"Unknown",
		"V4L",
		"FB",
		"ALSA",
		"DVB",
	};
	static const char *subdev_types[] = {
		"Unknown",
		"Video Decoder",
		"Video Encoder",
		"Miscellaneous",
	};

	switch (type) {
	case MEDIA_ENTITY_TYPE_NODE:
		if (subtype > 4)
			subtype = 0;
		return node_types[subtype];

	case MEDIA_ENTITY_TYPE_SUBDEV:
		if (subtype > 3)
			subtype = 0;
		return subdev_types[subtype];
	default:
		return node_types[0];
	}
}

/*
 * media_entity_remote_pad -
 */
struct media_entity_pad *media_entity_remote_pad(struct media_entity_pad *pad)
{
	unsigned int i;

	for (i = 0; i < pad->entity->info.links; ++i) {
		struct media_entity_link *link = &pad->entity->links[i];

		if (!(link->flags & MEDIA_LINK_FLAG_ACTIVE))
			continue;

		if (link->source == pad)
			return link->sink;

		if (link->sink == pad)
			return link->source;
	}

	return NULL;
}

/*
 * media_entity_by_name -
 */
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

/*
 * media_setup_link -
 */
int media_setup_link(struct media_device *media,
		     struct media_entity_pad *source,
		     struct media_entity_pad *sink,
		     __u32 flags)
{
	struct media_entity_link *link;
	struct media_user_link ulink;
	unsigned int i;
	int ret;

	for (i = 0; i < source->entity->info.links; i++) {
		link = &source->entity->links[i];

		if (link->source->entity == source->entity &&
		    link->source->index == source->index &&
		    link->sink->entity == sink->entity &&
		    link->sink->index == sink->index)
			break;
	}

	if (i == source->entity->info.links) {
		printf("%s: Link not found\n", __func__);
		return -EINVAL;
	}

	/* source pad */
	ulink.source.entity = source->entity->info.id;
	ulink.source.index = source->index;
	ulink.source.type = MEDIA_PAD_TYPE_OUTPUT;

	/* sink pad */
	ulink.sink.entity = sink->entity->info.id;
	ulink.sink.index = sink->index;
	ulink.sink.type = MEDIA_PAD_TYPE_INPUT;

	ulink.flags = flags | (link->flags & MEDIA_LINK_FLAG_IMMUTABLE);

	ret = ioctl(media->fd, MEDIA_IOC_SETUP_LINK, &ulink);
	if (ret < 0) {
		printf("%s: Unable to setup link (%s)\n", __func__,
			strerror(errno));
		return ret;
	}

	link->flags = flags;
	return 0;
}

int media_reset_links(struct media_device *media)
{
	unsigned int i, j;
	int ret;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		for (j = 0; j < entity->info.links; j++) {
			struct media_entity_link *link = &entity->links[j];

			if (link->flags & MEDIA_LINK_FLAG_IMMUTABLE)
				continue;

			ret = media_setup_link(media, link->source, link->sink,
					       link->flags & ~MEDIA_LINK_FLAG_ACTIVE);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static void media_print_topology_dot(struct media_device *media)
{
	unsigned int i, j;

	printf("digraph board {\n");
	printf("\trankdir=TB\n");

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];
		unsigned int npads;

		switch (entity->info.type) {
		case MEDIA_ENTITY_TYPE_NODE:
			printf("\tn%08x [label=\"%s\", shape=box, style=filled, "
			       "fillcolor=yellow]\n",
			       entity->info.id, entity->info.name);
			break;

		case MEDIA_ENTITY_TYPE_SUBDEV:
			printf("\tn%08x [label=\"{{", entity->info.id);

			for (j = 0, npads = 0; j < entity->info.pads; ++j) {
				if (entity->pads[j].type != MEDIA_PAD_TYPE_INPUT)
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("} | %s | {", entity->info.name);

			for (j = 0, npads = 0; j < entity->info.pads; ++j) {
				if (entity->pads[j].type != MEDIA_PAD_TYPE_OUTPUT)
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("}}\", shape=Mrecord, style=filled, fillcolor=green]\n");
			break;

		default:
			continue;
		}

		for (j = 0; j < entity->info.links; j++) {
			struct media_entity_link *link = &entity->links[j];

			if (link->source->entity != entity)
				continue;

			printf("\tn%08x", link->source->entity->info.id);
			if (link->source->entity->info.type == MEDIA_ENTITY_TYPE_SUBDEV)
				printf(":port%u", link->source->index);
			printf(" -> ");
			printf("n%08x", link->sink->entity->info.id);
			if (link->sink->entity->info.type == MEDIA_ENTITY_TYPE_SUBDEV)
				printf(":port%u", link->sink->index);

			if (link->flags & MEDIA_LINK_FLAG_IMMUTABLE)
				printf(" [style=bold]");
			else if (!(link->flags & MEDIA_LINK_FLAG_ACTIVE))
				printf(" [style=dashed]");
			printf("\n");
		}
	}

	printf("}\n");
}

static void media_print_topology_text(struct media_device *media)
{
	unsigned int i, j, k;
	unsigned int padding;

	printf("Device topology\n");

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		padding = printf("- entity %u: ", entity->info.id);
		printf("%s (%u pad%s, %u link%s)\n", entity->info.name,
			entity->info.pads, entity->info.pads > 1 ? "s" : "",
			entity->info.links, entity->info.links > 1 ? "s" : "");
		printf("%*ctype %s subtype %s\n", padding, ' ',
			media_entity_type_to_string(entity->info.type),
			media_entity_subtype_to_string(entity->info.type, entity->info.subtype));
		if (entity->devname)
			printf("%*cdevice node name %s\n", padding, ' ', entity->devname);

		for (j = 0; j < entity->info.pads; j++) {
			for (k = 0; k < entity->info.links; k++) {
				struct media_entity_link *link = &entity->links[k];

				if (link->source->entity != entity ||
				    link->source->index != j)
					continue;

				printf("\tpad%u -> '%s':pad%u [", link->source->index,
					link->sink->entity->info.name, link->sink->index);

				if (link->flags & MEDIA_LINK_FLAG_IMMUTABLE)
					printf("IMMUTABLE,");
				if (link->flags & MEDIA_LINK_FLAG_ACTIVE)
					printf("ACTIVE");

				printf("]\n");
			}
		}
		printf("\n");
	}
}

void media_print_topology(struct media_device *media, int dot)
{
	if (dot)
		media_print_topology_dot(media);
	else
		media_print_topology_text(media);
}

static int media_enum_links(struct media_device *media)
{
	__u32 id;
	int ret = 0;

	for (id = 1; id <= media->entities_count; id++) {
		struct media_entity *entity = &media->entities[id - 1];
		struct media_user_links links;
		unsigned int i;

		links.entity = entity->info.id;
		links.pads = malloc(entity->info.pads * sizeof(struct media_user_pad));
		links.links = malloc(entity->info.links * sizeof(struct media_user_link));

		if (ioctl(media->fd, MEDIA_IOC_ENUM_LINKS, &links) < 0) {
			printf("%s: Unable to enumerate pads and links (%s).\n",
				__func__, strerror(errno));
			free(links.pads);
			free(links.links);
			return -errno;
		}

		for (i = 0; i < entity->info.pads; ++i) {
			entity->pads[i].entity = entity;
			entity->pads[i].type = links.pads[i].type;
			entity->pads[i].index = links.pads[i].index;
		}

		for (i = 0; i < entity->info.links; ++i) {
			struct media_user_link *link = &links.links[i];
			struct media_entity *source;
			struct media_entity *sink;

			if (link->source.entity > media->entities_count ||
			    link->sink.entity > media->entities_count) {
				printf("WARNING entity %u link %u from %u/%u to %u/%u is invalid!\n",
					id, i, link->source.entity, link->source.index,
					link->sink.entity, link->sink.index);
				ret = -EINVAL;
			}

			source = &media->entities[link->source.entity - 1];
			sink = &media->entities[link->sink.entity - 1];
			entity->links[i].source = &source->pads[link->source.index];
			entity->links[i].sink = &sink->pads[link->sink.index];
			entity->links[i].flags = links.links[i].flags;
		}

		free(links.pads);
		free(links.links);
	}

	return ret;
}

static int media_enum_entities(struct media_device *media)
{
	struct media_entity *entity;
	struct stat devstat;
	char devname[32];
	unsigned int i;
	__u32 id;
	int ret;

	for (id = 1; ; id++) {
		media->entities =
			realloc(media->entities, id * sizeof(*media->entities));
		entity = &media->entities[id - 1];
		memset(entity, 0, sizeof(*entity));
		entity->fd = -1;
		entity->info.id = id;

		ret = ioctl(media->fd, MEDIA_IOC_ENUM_ENTITIES, &entity->info);
		if (ret < 0) {
			if (errno == EINVAL)
				break;
			return -errno;
		}

		entity->pads = malloc(entity->info.pads * sizeof(*entity->pads));
		entity->links = malloc(entity->info.links * sizeof(*entity->links));
		if (entity->pads == NULL || entity->links == NULL)
			return -ENOMEM;

		media->entities_count++;

		/* Find the corresponding device name. */
		if ((entity->info.type != MEDIA_ENTITY_TYPE_NODE ||
		     entity->info.type != MEDIA_NODE_TYPE_V4L) &&
		    (entity->info.type != MEDIA_ENTITY_TYPE_SUBDEV))
			continue;

		for (i = 0; i < 256; ++i) {
			if (entity->info.type == MEDIA_ENTITY_TYPE_NODE)
				sprintf(devname, "/dev/video%u", i);
			else
				sprintf(devname, "/dev/subdev%u", i);

			ret = stat(devname, &devstat);
			if (ret < 0)
				continue;

			if (major(devstat.st_rdev) == entity->info.v4l.major &&
			    minor(devstat.st_rdev) == entity->info.v4l.minor) {
				strcpy(entity->devname, devname);
				break;
			}
		}
	}

	return 0;
}

/*
 * media_open -
 */
struct media_device *media_open(const char *name, int verbose)
{
	struct media_device *media;
	int ret;

	media = malloc(sizeof(*media));
	if (media == NULL) {
		printf("%s: unable to allocate memory\n", __func__);
		return NULL;
	}
	memset(media, 0, sizeof(*media));

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

	ret = media_enum_entities(media);
	if (ret < 0) {
		printf("%s: Unable to enumerate entities for device %s (%s)\n",
			__func__, name, strerror(ret));
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

/*
 * media_close -
 */
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

