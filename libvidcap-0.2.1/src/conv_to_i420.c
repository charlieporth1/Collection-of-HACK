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

#include <string.h>
#include <vidcap/converters.h>
#include "logging.h"

/* NOTE: size of dest must be >= width * height * 3 / 2 
 */

/* Based on formulas found at http://en.wikipedia.org/wiki/YUV */
int
vidcap_rgb32_to_i420(int width, int height, const char * src, char * dst)
{
	unsigned char * dst_y_even;
	unsigned char * dst_y_odd;
	unsigned char * dst_u;
	unsigned char * dst_v;
	const unsigned char *src_even;
	const unsigned char *src_odd;
	int i, j;

	src_even = (const unsigned char *)src;
	src_odd = src_even + width * 4;

	dst_y_even = (unsigned char *)dst;
	dst_y_odd = dst_y_even + width;
	dst_u = dst_y_even + width * height;
	dst_v = dst_u + ((width * height) >> 2);

	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			short r, g, b;
			b = *src_even++;
			g = *src_even++;
			r = *src_even++;
			++src_even;
			*dst_y_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

			*dst_u++ = (( r * -38 - g * 74 + b * 112 + 128 ) >> 8 ) + 128;
			*dst_v++ = (( r * 112 - g * 94 - b * 18 + 128 ) >> 8 ) + 128;

			b = *src_even++;
			g = *src_even++;
			r = *src_even++;
			++src_even;
			*dst_y_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

			b = *src_odd++;
			g = *src_odd++;
			r = *src_odd++;
			++src_odd;
			*dst_y_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

			b = *src_odd++;
			g = *src_odd++;
			r = *src_odd++;
			++src_odd;
			*dst_y_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
		}

		dst_y_even += width;
		dst_y_odd += width;
		src_even += width * 4;
		src_odd += width * 4;
	}

	return 0;
}

int
vidcap_yuy2_to_i420(int width, int height, const char * src, char * dst)
{
	/* convert from a packed structure to a planar structure */
	char * dst_y_even = dst;
	char * dst_y_odd = dst + width;
	char * dst_u = dst + width * height;
	char * dst_v = dst_u + width * height / 4;
	const char * src_even = src;
	const char * src_odd = src + width * 2;

	int i, j;

	/* yuy2 has a vertical sampling period (for u and v)
	 * half that for i420. Will toss half of the
	 * U and V data during repackaging.
	 */
	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			*dst_y_even++ = *src_even++;
			*dst_u++      = *src_even++;
			*dst_y_even++ = *src_even++;
			*dst_v++      = *src_even++;

			*dst_y_odd++  = *src_odd++;
			src_odd++;
			*dst_y_odd++  = *src_odd++;
			src_odd++;
		}

		dst_y_even += width;
		dst_y_odd += width;
		src_even += width * 2;
		src_odd += width * 2;
	}

	return 0;
}

/* 2vuy is a byte reversal of yuy2, so the conversion is the
 * same except where we find the yuv components.
 */
int
conv_2vuy_to_i420(int width, int height, const char * src, char * dst)
{
	char * dst_y_even = dst;
	char * dst_y_odd = dst + width;
	char * dst_u = dst + width * height;
	char * dst_v = dst_u + width * height / 4;
	const char * src_even = src;
	const char * src_odd = src + width * 2;

	int i, j;

	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			*dst_u++      = *src_even++;
			*dst_y_even++ = *src_even++;
			*dst_v++      = *src_even++;
			*dst_y_even++ = *src_even++;

			src_odd++;
			*dst_y_odd++  = *src_odd++;
			src_odd++;
			*dst_y_odd++  = *src_odd++;
		}

		dst_y_even += width;
		dst_y_odd += width;
		src_even += width * 2;
		src_odd += width * 2;
	}

	return 0;
}

/* yvu9 is like i420, but the u and v planes
 * are reversed and 4x4 subsampled, instead of 2x2
 */
int
conv_yvu9_to_i420(int width, int height, const char * src, char * dst)
{
	char * dst_y = dst;
	char * dst_u_even = dst + width * height;
	char * dst_u_odd = dst_u_even + width / 2;
	char * dst_v_even = dst_u_even + width * height / 4;
	char * dst_v_odd = dst_v_even + width / 2;
	const char * src_y = src;
	const char * src_v = src_y + width * height;
	const char * src_u = src_v + width * height / 16;

	int i, j;

	memcpy(dst_y, src_y, height * width);

	for ( i = 0; i < height / 4; ++i )
	{
		for ( j = 0; j < width / 4; ++j )
		{
			*dst_u_even++ = *src_u;
			*dst_u_even++ = *src_u;
			*dst_u_odd++ = *src_u;
			*dst_u_odd++ = *src_u++;

			*dst_v_even++ = *src_v;
			*dst_v_even++ = *src_v;
			*dst_v_odd++ = *src_v;
			*dst_v_odd++ = *src_v++;
		}

		dst_u_even += width / 2;
		dst_u_odd += width / 2;
		dst_v_even += width / 2;
		dst_v_odd += width / 2;
	}

	return 0;
}
