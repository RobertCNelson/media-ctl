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

#ifndef __SUBDEV_H__
#define __SUBDEV_H__

#include <linux/v4l2-subdev.h>

struct media_entity;

const char *pixelcode_to_string(enum v4l2_mbus_pixelcode code);
enum v4l2_mbus_pixelcode string_to_pixelcode(const char *string,
					     unsigned int length);

int v4l2_subdev_get_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format which);
int v4l2_subdev_set_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format which);
int v4l2_subdev_get_crop(struct media_entity *entity, struct v4l2_rect *rect,
	unsigned int pad, enum v4l2_subdev_format which);
int v4l2_subdev_set_crop(struct media_entity *entity, struct v4l2_rect *rect,
	unsigned int pad, enum v4l2_subdev_format which);

#endif

