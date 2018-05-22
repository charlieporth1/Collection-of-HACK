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

#include <vidcap/vidcap.h>
#include "hotlist.h"

const struct hot_resolution hot_resolution_list[] =
{
	{ 640, 480 },
	{ 320, 240 },
	{ 160, 120 },
	{  80,  60 },
};

const int hot_resolution_list_len =
	sizeof(hot_resolution_list) / sizeof(struct hot_resolution);

const int hot_fourcc_list[] =
{
	VIDCAP_FOURCC_RGB32,
	VIDCAP_FOURCC_I420,
	VIDCAP_FOURCC_YUY2,
};

const int hot_fourcc_list_len =
	sizeof(hot_fourcc_list) / sizeof(int);

const struct hot_fps hot_fps_list[] =
{
	{ 15, 1 },
	{ 30, 1 },
	{ 2997, 100 },
	{ 10, 1 },
	{  1, 1 },
};

const int hot_fps_list_len =
	sizeof(hot_fps_list) / sizeof(struct hot_fps);

