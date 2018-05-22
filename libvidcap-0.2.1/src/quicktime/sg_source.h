/*
 * libvidcap - a cross-platform video capture library
 *
 * Copyright 2007 Wimba, Inc.
 *
 * Contributors:
 * Peter Grayson <jpgrayson@gmail.com>
 * Bill Cholewka <bcholew@gmail.com>
 *
 * libvidcap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * libvidcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _SG_SOURCE_H
#define _SG_SOURCE_H

#include <QuickTime/QuickTime.h>
#include "quicktime/gworld.h"

struct sg_source
{
	int device_id;
	int input_id;
	Str63 device_name;
	Str63 input_name;

	int acquired;
	int is_uvc;

	SeqGrabComponent grabber;
	SGChannel channel;

	DigitizerInfo info;
	long ms_per_frame;
	Fixed max_fps;
	long bytes_per_second;

	OSType native_pixel_format;

	TimeScale time_scale;

	struct gworld_info gi;
};

int sg_source_init(struct sg_source *, int device_id, int input_id,
		Str63 device_name, Str63 input_name);

int sg_source_anon_init(struct sg_source *);
int sg_source_destroy(struct sg_source *);

int sg_source_match(struct sg_source *, int device_id, int input_id);

int sg_source_acquire(struct sg_source *);
int sg_source_release(struct sg_source *);

int sg_source_format_validate(struct sg_source *, int width, int height,
		float fps);

int sg_source_format_set(struct sg_source *, int width, int height, float fps,
		OSType pixel_format);

int sg_source_image_desc_get(struct sg_source *, ImageDescriptionHandle);

int sg_source_capture_start(struct sg_source *, SGDataUPP, long);
int sg_source_capture_stop(struct sg_source *);
int sg_source_capture_poll(struct sg_source *);

#endif
