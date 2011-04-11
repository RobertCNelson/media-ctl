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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/v4l2-subdev.h>

#include "media.h"
#include "subdev.h"
#include "tools.h"

int v4l2_subdev_open(struct media_entity *entity)
{
	if (entity->fd != -1)
		return 0;

	entity->fd = open(entity->devname, O_RDWR);
	if (entity->fd == -1) {
		printf("%s: Failed to open subdev device node %s\n", __func__,
			entity->devname);
		return -errno;
	}

	return 0;
}

void v4l2_subdev_close(struct media_entity *entity)
{
	close(entity->fd);
	entity->fd = -1;
}

int v4l2_subdev_get_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_format fmt;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.pad = pad;
	fmt.which = which;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	if (ret < 0)
		return -errno;

	*format = fmt.format;
	return 0;
}

int v4l2_subdev_set_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_format fmt;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.pad = pad;
	fmt.which = which;
	fmt.format = *format;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	if (ret < 0)
		return -errno;

	*format = fmt.format;
	return 0;
}

int v4l2_subdev_get_crop(struct media_entity *entity, struct v4l2_rect *rect,
			 unsigned int pad, enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_crop crop;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&crop, 0, sizeof(crop));
	crop.pad = pad;
	crop.which = which;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_CROP, &crop);
	if (ret < 0)
		return -errno;

	*rect = crop.rect;
	return 0;
}

int v4l2_subdev_set_crop(struct media_entity *entity, struct v4l2_rect *rect,
			 unsigned int pad, enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_crop crop;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&crop, 0, sizeof(crop));
	crop.pad = pad;
	crop.which = which;
	crop.rect = *rect;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_CROP, &crop);
	if (ret < 0)
		return -errno;

	*rect = crop.rect;
	return 0;
}

int v4l2_subdev_get_frame_interval(struct media_entity *entity,
				   struct v4l2_fract *interval)
{
	struct v4l2_subdev_frame_interval ival;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&ival, 0, sizeof(ival));

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &ival);
	if (ret < 0)
		return -errno;

	*interval = ival.interval;
	return 0;
}

int v4l2_subdev_set_frame_interval(struct media_entity *entity,
				   struct v4l2_fract *interval)
{
	struct v4l2_subdev_frame_interval ival;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&ival, 0, sizeof(ival));
	ival.interval = *interval;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &ival);
	if (ret < 0)
		return -errno;

	*interval = ival.interval;
	return 0;
}
