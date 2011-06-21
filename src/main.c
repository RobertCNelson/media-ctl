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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>

#include "media.h"
#include "options.h"
#include "subdev.h"
#include "tools.h"

/* -----------------------------------------------------------------------------
 * Printing
 */

static struct {
	const char *name;
	enum v4l2_mbus_pixelcode code;
} mbus_formats[] = {
	{ "Y8", V4L2_MBUS_FMT_Y8_1X8},
	{ "Y10", V4L2_MBUS_FMT_Y10_1X10 },
	{ "Y12", V4L2_MBUS_FMT_Y12_1X12 },
	{ "YUYV", V4L2_MBUS_FMT_YUYV8_1X16 },
	{ "UYVY", V4L2_MBUS_FMT_UYVY8_1X16 },
	{ "SBGGR8", V4L2_MBUS_FMT_SBGGR8_1X8 },
	{ "SGBRG8", V4L2_MBUS_FMT_SGBRG8_1X8 },
	{ "SGRBG8", V4L2_MBUS_FMT_SGRBG8_1X8 },
	{ "SRGGB8", V4L2_MBUS_FMT_SRGGB8_1X8 },
	{ "SBGGR10", V4L2_MBUS_FMT_SBGGR10_1X10 },
	{ "SGBRG10", V4L2_MBUS_FMT_SGBRG10_1X10 },
	{ "SGRBG10", V4L2_MBUS_FMT_SGRBG10_1X10 },
	{ "SRGGB10", V4L2_MBUS_FMT_SRGGB10_1X10 },
	{ "SBGGR10_DPCM8", V4L2_MBUS_FMT_SBGGR10_DPCM8_1X8 },
	{ "SGBRG10_DPCM8", V4L2_MBUS_FMT_SGBRG10_DPCM8_1X8 },
	{ "SGRBG10_DPCM8", V4L2_MBUS_FMT_SGRBG10_DPCM8_1X8 },
	{ "SRGGB10_DPCM8", V4L2_MBUS_FMT_SRGGB10_DPCM8_1X8 },
	{ "SBGGR12", V4L2_MBUS_FMT_SBGGR12_1X12 },
	{ "SGBRG12", V4L2_MBUS_FMT_SGBRG12_1X12 },
	{ "SGRBG12", V4L2_MBUS_FMT_SGRBG12_1X12 },
	{ "SRGGB12", V4L2_MBUS_FMT_SRGGB12_1X12 },
};

static const char *pixelcode_to_string(enum v4l2_mbus_pixelcode code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mbus_formats); ++i) {
		if (mbus_formats[i].code == code)
			return mbus_formats[i].name;
	}

	return "unknown";
}

static enum v4l2_mbus_pixelcode string_to_pixelcode(const char *string,
					     unsigned int length)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mbus_formats); ++i) {
		if (strncmp(mbus_formats[i].name, string, length) == 0)
			break;
	}

	if (i == ARRAY_SIZE(mbus_formats))
		return (enum v4l2_mbus_pixelcode)-1;

	return mbus_formats[i].code;
}

static void v4l2_subdev_print_format(struct media_entity *entity,
	unsigned int pad, enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt format;
	struct v4l2_rect rect;
	int ret;

	ret = v4l2_subdev_get_format(entity, &format, pad, which);
	if (ret != 0)
		return;

	printf("[%s %ux%u", pixelcode_to_string(format.code),
	       format.width, format.height);

	ret = v4l2_subdev_get_crop(entity, &rect, pad, which);
	if (ret == 0)
		printf(" (%u,%u)/%ux%u", rect.left, rect.top,
		       rect.width, rect.height);
	printf("]");
}

static const char *media_entity_type_to_string(unsigned type)
{
	static const struct {
		__u32 type;
		const char *name;
	} types[] = {
		{ MEDIA_ENT_T_DEVNODE, "Node" },
		{ MEDIA_ENT_T_V4L2_SUBDEV, "V4L2 subdev" },
	};

	unsigned int i;

	type &= MEDIA_ENT_TYPE_MASK;

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		if (types[i].type == type)
			return types[i].name;
	}

	return "Unknown";
}

static const char *media_entity_subtype_to_string(unsigned type)
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
		"Sensor",
		"Flash",
		"Lens",
	};

	unsigned int subtype = type & MEDIA_ENT_SUBTYPE_MASK;

	switch (type & MEDIA_ENT_TYPE_MASK) {
	case MEDIA_ENT_T_DEVNODE:
		if (subtype >= ARRAY_SIZE(node_types))
			subtype = 0;
		return node_types[subtype];

	case MEDIA_ENT_T_V4L2_SUBDEV:
		if (subtype >= ARRAY_SIZE(subdev_types))
			subtype = 0;
		return subdev_types[subtype];
	default:
		return node_types[0];
	}
}

static const char *media_pad_type_to_string(unsigned flag)
{
	static const struct {
		__u32 flag;
		const char *name;
	} flags[] = {
		{ MEDIA_PAD_FL_SINK, "Input" },
		{ MEDIA_PAD_FL_SOURCE, "Output" },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (flags[i].flag & flag)
			return flags[i].name;
	}

	return "Unknown";
}

static void media_print_topology_dot(struct media_device *media)
{
	unsigned int i, j;

	printf("digraph board {\n");
	printf("\trankdir=TB\n");

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];
		unsigned int npads;

		switch (media_entity_type(entity)) {
		case MEDIA_ENT_T_DEVNODE:
			printf("\tn%08x [label=\"%s\\n%s\", shape=box, style=filled, "
			       "fillcolor=yellow]\n",
			       entity->info.id, entity->info.name, entity->devname);
			break;

		case MEDIA_ENT_T_V4L2_SUBDEV:
			printf("\tn%08x [label=\"{{", entity->info.id);

			for (j = 0, npads = 0; j < entity->info.pads; ++j) {
				if (!(entity->pads[j].flags & MEDIA_PAD_FL_SINK))
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("} | %s", entity->info.name);
			if (entity->devname)
				printf("\\n%s", entity->devname);
			printf(" | {");

			for (j = 0, npads = 0; j < entity->info.pads; ++j) {
				if (!(entity->pads[j].flags & MEDIA_PAD_FL_SOURCE))
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("}}\", shape=Mrecord, style=filled, fillcolor=green]\n");
			break;

		default:
			continue;
		}

		for (j = 0; j < entity->num_links; j++) {
			struct media_link *link = &entity->links[j];

			if (link->source->entity != entity)
				continue;

			printf("\tn%08x", link->source->entity->info.id);
			if (media_entity_type(link->source->entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				printf(":port%u", link->source->index);
			printf(" -> ");
			printf("n%08x", link->sink->entity->info.id);
			if (media_entity_type(link->sink->entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				printf(":port%u", link->sink->index);

			if (link->flags & MEDIA_LNK_FL_IMMUTABLE)
				printf(" [style=bold]");
			else if (!(link->flags & MEDIA_LNK_FL_ENABLED))
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
			entity->num_links, entity->num_links > 1 ? "s" : "");
		printf("%*ctype %s subtype %s\n", padding, ' ',
			media_entity_type_to_string(entity->info.type),
			media_entity_subtype_to_string(entity->info.type));
		if (entity->devname[0])
			printf("%*cdevice node name %s\n", padding, ' ', entity->devname);

		for (j = 0; j < entity->info.pads; j++) {
			struct media_pad *pad = &entity->pads[j];

			printf("\tpad%u: %s ", j, media_pad_type_to_string(pad->flags));

			if (media_entity_type(entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				v4l2_subdev_print_format(entity, j, V4L2_SUBDEV_FORMAT_ACTIVE);

			printf("\n");

			for (k = 0; k < entity->num_links; k++) {
				struct media_link *link = &entity->links[k];
				struct media_pad *source = link->source;
				struct media_pad *sink = link->sink;

				if (source->entity == entity && source->index == j)
					printf("\t\t-> '%s':pad%u [",
						sink->entity->info.name, sink->index);
				else if (sink->entity == entity && sink->index == j)
					printf("\t\t<- '%s':pad%u [",
						source->entity->info.name, source->index);
				else
					continue;

				if (link->flags & MEDIA_LNK_FL_IMMUTABLE)
					printf("IMMUTABLE,");
				if (link->flags & MEDIA_LNK_FL_ENABLED)
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

/* -----------------------------------------------------------------------------
 * Links setup
 */

static struct media_pad *parse_pad(struct media_device *media, const char *p, char **endp)
{
	unsigned int entity_id, pad;
	struct media_entity *entity;
	char *end;

	for (; isspace(*p); ++p);

	if (*p == '"') {
		for (end = (char *)p + 1; *end && *end != '"'; ++end);
		if (*end != '"')
			return NULL;

		entity = media_get_entity_by_name(media, p + 1, end - p - 1);
		if (entity == NULL)
			return NULL;

		++end;
	} else {
		entity_id = strtoul(p, &end, 10);
		entity = media_get_entity_by_id(media, entity_id);
		if (entity == NULL)
			return NULL;
	}
	for (; isspace(*end); ++end);

	if (*end != ':')
		return NULL;
	for (p = end + 1; isspace(*p); ++p);

	pad = strtoul(p, &end, 10);
	for (p = end; isspace(*p); ++p);

	if (pad >= entity->info.pads)
		return NULL;

	for (p = end; isspace(*p); ++p);
	if (endp)
		*endp = (char *)p;

	return &entity->pads[pad];
}

static struct media_link *parse_link(struct media_device *media, const char *p, char **endp)
{
	struct media_link *link;
	struct media_pad *source;
	struct media_pad *sink;
	unsigned int i;
	char *end;

	source = parse_pad(media, p, &end);
	if (source == NULL)
		return NULL;

	if (end[0] != '-' || end[1] != '>')
		return NULL;
	p = end + 2;

	sink = parse_pad(media, p, &end);
	if (sink == NULL)
		return NULL;

	*endp = end;

	for (i = 0; i < source->entity->num_links; i++) {
		link = &source->entity->links[i];

		if (link->source == source && link->sink == sink)
			return link;
	}

	return NULL;
}

static int setup_link(struct media_device *media, const char *p, char **endp)
{
	struct media_link *link;
	__u32 flags;
	char *end;

	link = parse_link(media, p, &end);
	if (link == NULL) {
		printf("Unable to parse link\n");
		return -EINVAL;
	}

	p = end;
	if (*p++ != '[') {
		printf("Unable to parse link flags\n");
		return -EINVAL;
	}

	flags = strtoul(p, &end, 10);
	for (p = end; isspace(*p); p++);
	if (*p++ != ']') {
		printf("Unable to parse link flags\n");
		return -EINVAL;
	}

	for (; isspace(*p); p++);
	*endp = (char *)p;

	printf("Setting up link %u:%u -> %u:%u [%u]\n",
		link->source->entity->info.id, link->source->index,
		link->sink->entity->info.id, link->sink->index,
		flags);

	return media_setup_link(media, link->source, link->sink, flags);
}

static int setup_links(struct media_device *media, const char *p)
{
	char *end;
	int ret;

	do {
		ret = setup_link(media, p, &end);
		if (ret < 0)
			return ret;

		p = end + 1;
	} while (*end == ',');

	return *end ? -EINVAL : 0;
}

/* -----------------------------------------------------------------------------
 * Formats setup
 */

static int parse_format(struct v4l2_mbus_framefmt *format, const char *p, char **endp)
{
	enum v4l2_mbus_pixelcode code;
	unsigned int width, height;
	char *end;

	for (; isspace(*p); ++p);
	for (end = (char *)p; !isspace(*end) && *end != '\0'; ++end);

	code = string_to_pixelcode(p, end - p);
	if (code == (enum v4l2_mbus_pixelcode)-1)
		return -EINVAL;

	for (p = end; isspace(*p); ++p);
	width = strtoul(p, &end, 10);
	if (*end != 'x')
		return -EINVAL;

	p = end + 1;
	height = strtoul(p, &end, 10);
	*endp = end;

	memset(format, 0, sizeof(*format));
	format->width = width;
	format->height = height;
	format->code = code;

	return 0;
}

static int parse_crop(struct v4l2_rect *crop, const char *p, char **endp)
{
	char *end;

	if (*p++ != '(')
		return -EINVAL;

	crop->left = strtoul(p, &end, 10);
	if (*end != ',')
		return -EINVAL;

	p = end + 1;
	crop->top = strtoul(p, &end, 10);
	if (*end++ != ')')
		return -EINVAL;
	if (*end != '/')
		return -EINVAL;

	p = end + 1;
	crop->width = strtoul(p, &end, 10);
	if (*end != 'x')
		return -EINVAL;

	p = end + 1;
	crop->height = strtoul(p, &end, 10);
	*endp = end;

	return 0;
}

static int parse_frame_interval(struct v4l2_fract *interval, const char *p, char **endp)
{
	char *end;

	for (; isspace(*p); ++p);

	interval->numerator = strtoul(p, &end, 10);

	for (p = end; isspace(*p); ++p);
	if (*p++ != '/')
		return -EINVAL;

	for (; isspace(*p); ++p);
	interval->denominator = strtoul(p, &end, 10);

	*endp = end;
	return 0;
}

static struct media_pad *parse_pad_format(struct media_device *media,
	struct v4l2_mbus_framefmt *format, struct v4l2_rect *crop,
	struct v4l2_fract *interval, const char *p, char **endp)
{
	struct media_pad *pad;
	char *end;
	int ret;

	for (; isspace(*p); ++p);

	pad = parse_pad(media, p, &end);
	if (pad == NULL)
		return NULL;

	for (p = end; isspace(*p); ++p);
	if (*p++ != '[')
		return NULL;

	for (; isspace(*p); ++p);

	if (isalnum(*p)) {
		ret = parse_format(format, p, &end);
		if (ret < 0)
			return NULL;

		for (p = end; isspace(*p); p++);
	}

	if (*p == '(') {
		ret = parse_crop(crop, p, &end);
		if (ret < 0)
			return NULL;

		for (p = end; isspace(*p); p++);
	}

	if (*p == '@') {
		ret = parse_frame_interval(interval, ++p, &end);
		if (ret < 0)
			return NULL;

		for (p = end; isspace(*p); p++);
	}

	if (*p != ']')
		return NULL;

	*endp = (char *)p + 1;
	return pad;
}

static int set_format(struct media_pad *pad, struct v4l2_mbus_framefmt *format)
{
	int ret;

	if (format->width == 0 || format->height == 0)
		return 0;

	printf("Setting up format %s %ux%u on pad %s/%u\n",
		pixelcode_to_string(format->code), format->width, format->height,
		pad->entity->info.name, pad->index);

	ret = v4l2_subdev_set_format(pad->entity, format, pad->index,
				     V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret < 0) {
		printf("Unable to set format: %s (%d)\n", strerror(-ret), ret);
		return ret;
	}

	printf("Format set: %s %ux%u\n",
		pixelcode_to_string(format->code), format->width, format->height);

	return 0;
}

static int set_crop(struct media_pad *pad, struct v4l2_rect *crop)
{
	int ret;

	if (crop->left == -1 || crop->top == -1)
		return 0;

	printf("Setting up crop rectangle (%u,%u)/%ux%u on pad %s/%u\n",
		crop->left, crop->top, crop->width, crop->height,
		pad->entity->info.name, pad->index);

	ret = v4l2_subdev_set_crop(pad->entity, crop, pad->index,
				   V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret < 0) {
		printf("Unable to set crop rectangle: %s (%d)\n", strerror(-ret), ret);
		return ret;
	}

	printf("Crop rectangle set: (%u,%u)/%ux%u\n",
		crop->left, crop->top, crop->width, crop->height);

	return 0;
}

static int set_frame_interval(struct media_entity *entity, struct v4l2_fract *interval)
{
	int ret;

	if (interval->numerator == 0)
		return 0;

	printf("Setting up frame interval %u/%u on entity %s\n",
		interval->numerator, interval->denominator, entity->info.name);

	ret = v4l2_subdev_set_frame_interval(entity, interval);
	if (ret < 0) {
		printf("Unable to set frame interval: %s (%d)", strerror(-ret), ret);
		return ret;
	}

	printf("Frame interval set: %u/%u\n",
		interval->numerator, interval->denominator);

	return 0;
}


static int setup_format(struct media_device *media, const char *p, char **endp)
{
	struct v4l2_mbus_framefmt format = { 0, 0, 0 };
	struct media_pad *pad;
	struct v4l2_rect crop = { -1, -1, -1, -1 };
	struct v4l2_fract interval = { 0, 0 };
	unsigned int i;
	char *end;
	int ret;

	pad = parse_pad_format(media, &format, &crop, &interval, p, &end);
	if (pad == NULL) {
		printf("Unable to parse format\n");
		return -EINVAL;
	}

	if (pad->flags & MEDIA_PAD_FL_SOURCE) {
		ret = set_crop(pad, &crop);
		if (ret < 0)
			return ret;
	}

	ret = set_format(pad, &format);
	if (ret < 0)
		return ret;

	if (pad->flags & MEDIA_PAD_FL_SINK) {
		ret = set_crop(pad, &crop);
		if (ret < 0)
			return ret;
	}

	ret = set_frame_interval(pad->entity, &interval);
	if (ret < 0)
		return ret;


	/* If the pad is an output pad, automatically set the same format on
	 * the remote subdev input pads, if any.
	 */
	if (pad->flags & MEDIA_PAD_FL_SOURCE) {
		for (i = 0; i < pad->entity->num_links; ++i) {
			struct media_link *link = &pad->entity->links[i];
			struct v4l2_mbus_framefmt remote_format;

			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				continue;

			if (link->source == pad &&
			    link->sink->entity->info.type == MEDIA_ENT_T_V4L2_SUBDEV) {
				remote_format = format;
				set_format(link->sink, &remote_format);
			}
		}
	}

	*endp = end;
	return 0;
}

static int setup_formats(struct media_device *media, const char *p)
{
	char *end;
	int ret;

	do {
		ret = setup_format(media, p, &end);
		if (ret < 0)
			return ret;

		p = end + 1;
	} while (*end == ',');

	return *end ? -EINVAL : 0;
}

int main(int argc, char **argv)
{
	struct media_device *media;
	int ret = -1;

	if (parse_cmdline(argc, argv))
		return EXIT_FAILURE;

	/* Open the media device and enumerate entities, pads and links. */
	media = media_open(media_opts.devname, media_opts.verbose);
	if (media == NULL)
		goto out;

	if (media_opts.entity) {
		struct media_entity *entity;

		entity = media_get_entity_by_name(media, media_opts.entity,
						  strlen(media_opts.entity));
		if (entity == NULL) {
			printf("Entity '%s' not found\n", media_opts.entity);
			goto out;
		}

		printf("%s\n", entity->devname);
	}

	if (media_opts.pad) {
		struct media_pad *pad;

		pad = parse_pad(media, media_opts.pad, NULL);
		if (pad == NULL) {
			printf("Pad '%s' not found\n", media_opts.pad);
			goto out;
		}

		v4l2_subdev_print_format(pad->entity, pad->index,
					 V4L2_SUBDEV_FORMAT_ACTIVE);
		printf("\n");
	}

	if (media_opts.print || media_opts.print_dot) {
		media_print_topology(media, media_opts.print_dot);
		printf("\n");
	}

	if (media_opts.reset) {
		printf("Resetting all links to inactive\n");
		media_reset_links(media);
	}

	if (media_opts.links)
		setup_links(media, media_opts.links);

	if (media_opts.formats)
		setup_formats(media, media_opts.formats);

	if (media_opts.interactive) {
		while (1) {
			char buffer[32];
			char *end;

			printf("Enter a link to modify or enter to stop\n");
			if (fgets(buffer, sizeof buffer, stdin) == NULL)
				break;

			if (buffer[0] == '\n')
				break;

			setup_link(media, buffer, &end);
		}
	}

	ret = 0;

out:
	if (media)
		media_close(media);

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}

