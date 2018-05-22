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

#include <vidcap/converters.h>

/* NOTE: size of dest buffer must be >= width * height * 2 */

/* Based on formulas found at http://en.wikipedia.org/wiki/YUV */
int
vidcap_rgb32_to_yuy2(int width, int height, const char * src, char * dest)
{
	const unsigned char * src_even = (const unsigned char *)src;
	const unsigned char * src_odd = src_even + width * 4;
	unsigned char * dst_even = (unsigned char *)dest;
	unsigned char * dst_odd = dst_even + width * 2;
	int i;

	for ( i = 0; i < height / 2; ++i )
	{
		int j;
		for ( j = 0; j < width / 2; ++j )
		{
			/* NOTE: u and v are taken from different src samples */

			short r, g, b;
			b = *src_even++;
			g = *src_even++;
			r = *src_even++;
			++src_even;
			*dst_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
			*dst_even++ = (( r * -38 - g * 74 + b * 112 + 128 ) >> 8 ) + 128;

			b = *src_even++;
			g = *src_even++;
			r = *src_even++;
			++src_even;
			*dst_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
			*dst_even++ = (( r * 112 - g * 94 - b * 18 + 128 ) >> 8 ) + 128;

			b = *src_odd++;
			g = *src_odd++;
			r = *src_odd++;
			++src_odd;
			*dst_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
			*dst_odd++ = (( r * -38 - g * 74 + b * 112 + 128 ) >> 8 ) + 128;

			b = *src_odd++;
			g = *src_odd++;
			r = *src_odd++;
			++src_odd;
			*dst_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
			*dst_odd++ = (( r * 112 - g * 94 - b * 18 + 128 ) >> 8 ) + 128;
		}

		dst_even += width * 2;
		dst_odd += width * 2;
		src_even += width * 4;
		src_odd += width * 4;
	}

	return 0;
}

int
vidcap_i420_to_yuy2(int width, int height, const char * src, char * dest)
{
	/* convert from a planar structure to a packed structure */
	const char * src_y_even = src;
	const char * src_y_odd = src + width;
	const char * src_u = src + width * height;
	const char * src_v = src_u + width * height / 4;
	char * dst_even = dest;
	char * dst_odd = dest + width * 2;

	int i, j;

	/* i420 has a vertical sampling period (for u and v)
	 * double that for yuy2. Will re-use
	 * U and V data during repackaging.
	 */
	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			*dst_even++ = *src_y_even++;
			*dst_odd++  = *src_y_odd++;

			*dst_even++ = *src_u;
			*dst_odd++  = *src_u++;

			*dst_even++ = *src_y_even++;
			*dst_odd++  = *src_y_odd++;

			*dst_even++ = *src_v;
			*dst_odd++  = *src_v++;
		}

		src_y_even += width;
		src_y_odd += width;
		dst_even += width * 2;
		dst_odd += width * 2;
	}

	return 0;
}

int
conv_2vuy_to_yuy2(int width, int height, const char * src, char * dest)
{
	int i;
	unsigned int * d = (unsigned int *)dest;
	const unsigned int * s = (const unsigned int *)src;

	for ( i = 0; i < width * height / 2; ++i )
	{
		*d++ = ((*s & 0xff000000) >> 8) |
			((*s & 0x00ff0000) << 8) |
			((*s & 0x0000ff00) >> 8) |
			((*s & 0x000000ff) << 8);
		++s;
	}

	return 0;
}
