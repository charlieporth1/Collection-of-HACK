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

#include "string.h"
#include "conv.h"
#include "logging.h"

int conv_2vuy_to_i420(int w, int h, const char * s, char * d);
int conv_2vuy_to_yuy2(int w, int h, const char * s, char * d);
int conv_rgb24_to_rgb32(int w, int h, const char * s, char * d);
int conv_yvu9_to_i420(int w, int h, const char * s, char * d);
int conv_bottom_up_rgb24_to_rgb32(int w, int h, const char * s, char * d);

struct conv_info
{
	int src_fourcc;
	int dst_fourcc;
	conv_func func;
	const char *name;
};

static const struct conv_info conv_list[] =
{
	{ VIDCAP_FOURCC_I420,  VIDCAP_FOURCC_RGB32, vidcap_i420_to_rgb32,
		"i420->rgb32" },
	{ VIDCAP_FOURCC_YUY2,  VIDCAP_FOURCC_RGB32, vidcap_yuy2_to_rgb32,
		"yuy2->rgb32" },
	{ VIDCAP_FOURCC_RGB32, VIDCAP_FOURCC_I420,  vidcap_rgb32_to_i420,
		"rgb32->i420" },
	{ VIDCAP_FOURCC_YUY2,  VIDCAP_FOURCC_I420,  vidcap_yuy2_to_i420,
		"yuy2->i420" },
	{ VIDCAP_FOURCC_I420,  VIDCAP_FOURCC_YUY2,  vidcap_i420_to_yuy2,
		"i420->yuy2" },
	{ VIDCAP_FOURCC_RGB32, VIDCAP_FOURCC_YUY2,  vidcap_rgb32_to_yuy2,
		"rgb32->yuy2" },

	{ VIDCAP_FOURCC_2VUY,  VIDCAP_FOURCC_YUY2,  conv_2vuy_to_yuy2,
		"2vuy->yuy2" },
	{ VIDCAP_FOURCC_2VUY,  VIDCAP_FOURCC_I420,  conv_2vuy_to_i420,
		"2vuy->i420" },
	{ VIDCAP_FOURCC_RGB24, VIDCAP_FOURCC_RGB32, conv_rgb24_to_rgb32,
		"rgb24->rgb32" },
	{ VIDCAP_FOURCC_BOTTOM_UP_RGB24, VIDCAP_FOURCC_RGB32,
		conv_bottom_up_rgb24_to_rgb32, "bottom-up rgb24->rgb32" },

	{ VIDCAP_FOURCC_YVU9,  VIDCAP_FOURCC_I420,  conv_yvu9_to_i420,
		"yvu9->i420" },
};

static const int conv_list_len = sizeof(conv_list) / sizeof(struct conv_info);

static int
destride_packed(int image_byte_width, int height, int stride,
		const char * src, char * dst)
{
	/* destride a packed structure */
	char * dst_even = dst;
	char * dst_odd = dst + image_byte_width;
	const char * src_even = src;
	const char * src_odd = src + stride;

	int i;
	for ( i = 0; i < height / 2; ++i )
	{
		memcpy(dst_even, src_even, image_byte_width);
		memcpy(dst_odd, src_odd, image_byte_width);
		
		dst_even += 2 * image_byte_width;
		dst_odd += 2 * image_byte_width;
		src_even += 2 * stride;
		src_odd += 2 * stride;
	}

	return 0;
}

static int
destride_planar(int width, int height,
		int u_width, int u_height,
		int v_width, int v_height,
		int y_stride, int u_stride, int v_stride,
		const char * src, char * dst)
{
	char * dst_y_even = dst;
	char * dst_y_odd = dst + width;
	char * dst_u_even = dst + width * height;
	char * dst_u_odd = dst_u_even + u_width;
	char * dst_v_even = dst_u_even + u_width * u_height;
	char * dst_v_odd = dst_v_even + v_width;
	const char * src_y_even = src;
	const char * src_y_odd = src + y_stride;
	const char * src_u_even = src + y_stride * height;
	const char * src_u_odd = src_u_even + u_stride;
	const char * src_v_even = src_u_even + u_stride * u_height;
	const char * src_v_odd = src_v_even + v_stride;

	int i;

	for ( i = 0; i < height / 2; ++i )
	{
		memcpy(dst_y_even, src_y_even, width);
		memcpy(dst_y_odd, src_y_odd, width);

		dst_y_even += 2 * width;
		dst_y_odd += 2 * width;
		src_y_even += 2 * y_stride;
		src_y_odd += 2 * y_stride;
	}

	for ( i = 0; i < u_height / 2; ++i )
	{
		memcpy(dst_u_even, src_u_even, u_width);
		memcpy(dst_u_odd, src_u_odd, u_width);

		dst_u_even += 2 * u_width;
		dst_u_odd += 2 * u_width;
		src_u_even += 2 * u_stride;
		src_u_odd += 2 * u_stride;
	}

	for ( i = 0; i < v_height / 2; ++i )
	{
		memcpy(dst_v_even, src_v_even, v_width);
		memcpy(dst_v_odd, src_v_odd, v_width);

		dst_v_even += 2 * v_width;
		dst_v_odd += 2 * v_width;
		src_v_even += 2 * v_stride;
		src_v_odd += 2 * v_stride;
	}

	return 0;
}

int
destridify(int width, int height, int fourcc, int stride,
		const char * src, char * dst)
{
	/* NOTE: we're making-up the u and v strides for planar formats,
	 *       rather than pass them in.
	 */

	switch ( fourcc )
	{
	case VIDCAP_FOURCC_I420:
		log_error("UNTESTED: destriding i420\n");
		/* FIXME: only destride if necessary */
		return destride_planar(width, height,
				/* fix these u and v strides to be 32-bit aligned? */
				width / 2, height / 2, width / 2, height / 2,
				stride, stride / 2, stride / 2,
				src, dst);
		break;
	case VIDCAP_FOURCC_YUY2:
		if ( stride == 2 * width )
			return -1;
		return destride_packed(2 * width, height, stride, src, dst);
		break;
	case VIDCAP_FOURCC_RGB32:
		if ( stride == 4 * width )
			return -1;
		return destride_packed(4 * width, height, stride, src, dst);
		break;
	case VIDCAP_FOURCC_2VUY:
		if ( stride == 2 * width )
			return -1;
		return destride_packed(2 * width, height, stride, src, dst);
		break;
	case VIDCAP_FOURCC_RGB24:
		if ( stride == 3 * width )
			return -1;
		return destride_packed(3 * width, height, stride, src, dst);
		break;
	case VIDCAP_FOURCC_BOTTOM_UP_RGB24:
		if ( stride == 3 * width )
			return -1;
		return destride_packed(3 * width, height, stride, src, dst);
		break;
	case VIDCAP_FOURCC_YVU9:
		log_error("UNTESTED: destriding yvu9\n");
		/* FIXME: only destride if necessary */
		return destride_planar(width, height,
				width / 4, height / 4, width / 4, height / 4,
				/* fix u and v strides to be 32-bit aligned? */
				stride, stride / 4, stride / 4,
				src, dst);
		break;
	default:
		log_error("Invalid fourcc [%s]\n",
				vidcap_fourcc_string_get(fourcc));
		return -1;
		break;
	}
}

conv_func
conv_conversion_func_get(int src_fourcc, int dst_fourcc)
{
	int i;

	for ( i = 0; i < conv_list_len; ++i )
	{
		const struct conv_info * ci = &conv_list[i];

		if ( src_fourcc == ci->src_fourcc &&
				dst_fourcc == ci->dst_fourcc )
			return ci->func;
	}

	return 0;
}

int
conv_fmt_size_get(int width, int height, int fourcc)
{
	const int pixels = width * height;

	switch ( fourcc )
	{
	case VIDCAP_FOURCC_I420:
		return pixels * 3 / 2;

	case VIDCAP_FOURCC_RGB24:
	case VIDCAP_FOURCC_BOTTOM_UP_RGB24:
		return pixels * 3;

	case VIDCAP_FOURCC_RGB32:
		return pixels * 4;

	case VIDCAP_FOURCC_RGB555:
	case VIDCAP_FOURCC_YUY2:
	case VIDCAP_FOURCC_2VUY:
		return pixels * 2;
	default:
		return 0;
	}
}

const char *
conv_conversion_name_get(conv_func function)
{
	int i;

	for ( i = 0; i < conv_list_len; ++i )
	{
		const struct conv_info * ci = &conv_list[i];

		if ( ci->func == function )
			return ci->name;
	}

	return "(ERROR: Conversion name not found)";
}
