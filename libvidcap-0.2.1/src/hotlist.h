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

#ifndef _HOTLIST_H
#define _HOTLIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct hot_resolution
{
	int width;
	int height;
};

struct hot_fps
{
	int fps_numerator;
	int fps_denominator;
};

extern const struct hot_resolution hot_resolution_list[];
extern const int hot_resolution_list_len;

extern const int hot_fourcc_list[];
extern const int hot_fourcc_list_len;

extern const struct hot_fps hot_fps_list[];
extern const int hot_fps_list_len;

#ifdef __cplusplus
}
#endif

#endif
