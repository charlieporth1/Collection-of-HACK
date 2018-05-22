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

enum {
	CLIP_SIZE = 811,
	CLIP_OFFSET = 277,
	YMUL = 298,
	RMUL = 409,
	BMUL = 516,
	G1MUL = -100,
	G2MUL = -208,
};

static int tables_initialized = 0;

static int yuv2rgb_y[256];
static int yuv2rgb_r[256];
static int yuv2rgb_b[256];
static int yuv2rgb_g1[256];
static int yuv2rgb_g2[256];

static unsigned long yuv2rgb_clip[CLIP_SIZE];
static unsigned long yuv2rgb_clip8[CLIP_SIZE];
static unsigned long yuv2rgb_clip16[CLIP_SIZE];

#define COMPOSE_RGB(yc, rc, gc, bc)		\
	( 0xff000000 |				\
	  yuv2rgb_clip16[(yc) + (rc)] |		\
	  yuv2rgb_clip8[(yc) + (gc)] |		\
	  yuv2rgb_clip[(yc) + (bc)] )

static void init_yuv2rgb_tables(void)
{
	int i;

	for ( i = 0; i < 256; ++i )
	{
		yuv2rgb_y[i] = (YMUL * (i - 16) + 128) >> 8;
		yuv2rgb_r[i] = (RMUL * (i - 128)) >> 8;
		yuv2rgb_b[i] = (BMUL * (i - 128)) >> 8;
		yuv2rgb_g1[i] = (G1MUL * (i - 128)) >> 8;
		yuv2rgb_g2[i] = (G2MUL * (i - 128)) >> 8;
	}
	for ( i = 0 ; i < CLIP_OFFSET ; ++i )
	{
		yuv2rgb_clip[i] = 0;
		yuv2rgb_clip8[i] = 0;
		yuv2rgb_clip16[i] = 0;
	}
	for ( ; i < CLIP_OFFSET + 256 ; ++i )
	{
		yuv2rgb_clip[i] = i - CLIP_OFFSET;
		yuv2rgb_clip8[i] = (i - CLIP_OFFSET) << 8;
		yuv2rgb_clip16[i] = (i - CLIP_OFFSET) << 16;
	}
	for ( ; i < CLIP_SIZE ; ++i )
	{
		yuv2rgb_clip[i] = 255;
		yuv2rgb_clip8[i] = 255 << 8;
		yuv2rgb_clip16[i] = 255 << 16;
	}

	tables_initialized = 1;
}

/*
 * Function to convert i420 images to rgb32
 * rgb32: 0xFFRRGGBB
 * This function uses precalculated tables that are initialized
 * on the first run.
 *
 * Dest should be width * height * 4 bytes in size.
 *
 * Based on the formulas found at http://en.wikipedia.org/wiki/YUV
 *
 * NOTE: size of dest buffer must be >= width * height * 4
 */

int
vidcap_i420_to_rgb32(int width, int height, const char * src, char * dest)
{
	const unsigned char * y_even;
	const unsigned char * y_odd;
	const unsigned char * u;
	const unsigned char * v;
	unsigned int *dst_even;
	unsigned int *dst_odd;
	int i, j;

	if ( !tables_initialized )
		init_yuv2rgb_tables();

	dst_even = (unsigned int *)dest;
	dst_odd = dst_even + width;

	y_even = (const unsigned char *)src;
	y_odd = y_even + width;
	u = y_even + width * height;
	v = u + ((width * height) >> 2);

	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			const int rc = yuv2rgb_r[*v];
			const int gc = yuv2rgb_g1[*v] + yuv2rgb_g2[*u];
			const int bc = yuv2rgb_b[*u];
			const int yc0_even = CLIP_OFFSET + yuv2rgb_y[*y_even++];
			const int yc1_even = CLIP_OFFSET + yuv2rgb_y[*y_even++];
			const int yc0_odd = CLIP_OFFSET + yuv2rgb_y[*y_odd++];
			const int yc1_odd = CLIP_OFFSET + yuv2rgb_y[*y_odd++];

			*dst_even++ = COMPOSE_RGB(yc0_even, rc, gc, bc);
			*dst_even++ = COMPOSE_RGB(yc1_even, rc, gc, bc);
			*dst_odd++ = COMPOSE_RGB(yc0_odd, rc, gc, bc);
			*dst_odd++ = COMPOSE_RGB(yc1_odd, rc, gc, bc);
			
			++u;
			++v;
		}

		y_even += width;
		y_odd += width;
		dst_even += width;
		dst_odd += width;
	}

	return 0;
}

/* There are two pixels per 32-bit word of the source buffer. This
 * includes two luminance (y) components and a single red and blue
 * chroma (Cr and Cb aka v and u) sample use for both pixels. 
 */
int vidcap_yuy2_to_rgb32(int width, int height, const char * src, char * dest)
{
	unsigned int * d = (unsigned int *)dest;
	int i, j;

	if ( !tables_initialized )
		init_yuv2rgb_tables();

	for ( i = 0; i < height; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			const unsigned char y0 = *src++;
			const unsigned char u  = *src++;
			const unsigned char y1 = *src++;
			const unsigned char v  = *src++;
			const int rc = yuv2rgb_r[v];
			const int gc = yuv2rgb_g1[v] + yuv2rgb_g2[u];
			const int bc = yuv2rgb_b[u];
			const int yc0 = CLIP_OFFSET + yuv2rgb_y[y0];
			const int yc1 = CLIP_OFFSET + yuv2rgb_y[y1];

			*d++ = COMPOSE_RGB(yc0, rc, gc, bc);
			*d++ = COMPOSE_RGB(yc1, rc, gc, bc);
		}
	}

	return 0;
}

int conv_rgb24_to_rgb32(int width, int height, const char * src, char * dest)
{
	int i;
	unsigned int * d = (unsigned int *)dest;
	const unsigned char * s = (const unsigned char *)src;

	for ( i = 0; i < width * height; ++i )
	{
		*d = 0xff000000;
		*d |= *s++;           /* blue */
		*d |= (*s++) << 8;    /* green */
		*d++ |= (*s++) << 16; /* red */
	}

	return 0;
}

int conv_bottom_up_rgb24_to_rgb32(int width, int height,
		const char * src, char * dest)
{
	int i, j;
	unsigned int * d = (unsigned int *)dest;
	const unsigned char * src_bot = (const unsigned char *)src +
		width * (height - 1) * 3;

	for ( i = 0; i < height; ++i )
	{
		for ( j = 0; j < width; ++j )
		{
			*d = 0xff000000;
			*d |= (*src_bot++) << 0;     /* red */
			*d |= (*src_bot++) << 8;     /* green */
			*d++ |= (*src_bot++) << 16;  /* blue */
		}

		src_bot -= width * 6;
	}

	return 0;
}
