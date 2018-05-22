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

#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "sapi_context.h"
#include "sapi.h"

typedef int sapi_initializer_t(struct sapi_context *);

#ifdef HAVE_V4L
int sapi_v4l_initialize(struct sapi_context *);
#endif
#ifdef HAVE_QUICKTIME
int sapi_qt_initialize(struct sapi_context *);
#endif
#ifdef HAVE_DIRECTSHOW
int sapi_dshow_initialize(struct sapi_context *);
#endif

static sapi_initializer_t * const sapi_initializers[] =
{
#ifdef HAVE_V4L
	sapi_v4l_initialize,
#endif
#ifdef HAVE_QUICKTIME
	sapi_qt_initialize,
#endif
#ifdef HAVE_DIRECTSHOW
	sapi_dshow_initialize,
#endif
	0
};

struct vidcap_context
{
	int num_sapi;
	struct sapi_context * sapi_list;
};

vidcap_state *
vidcap_initialize(void)
{
	struct vidcap_context * vcc;
	int i;

	vcc = calloc(1, sizeof(*vcc));

	if ( !vcc )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	vcc->num_sapi = 0;

	while ( sapi_initializers[vcc->num_sapi] )
		vcc->num_sapi++;

	vcc->sapi_list = calloc(vcc->num_sapi, sizeof(struct sapi_context));

	if ( !vcc->sapi_list )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	for ( i = 0; i < vcc->num_sapi; ++i )
	{
		if ( sapi_initializers[i](&vcc->sapi_list[i]) )
		{
			log_error("failed to initialize sapi %d\n", i);
			goto fail;
		}
	}

	return vcc;

fail:
	free(vcc->sapi_list);
	free(vcc);
	return 0;
}

void
vidcap_destroy(vidcap_state * state)
{
	struct vidcap_context * vcc = (struct vidcap_context *)state;

	int i;
	for ( i = 0; i < vcc->num_sapi; ++i )
	{
		struct sapi_context * sapi_ctx = &vcc->sapi_list[i];

		while ( sapi_ctx->ref_count )
			sapi_ctx->release(sapi_ctx);

		sapi_ctx->destroy(sapi_ctx);
	}

	free(vcc->sapi_list);
	free(vcc);
}

int
vidcap_log_level_set(enum vidcap_log_level level)
{
	int log_level;

	switch ( level )
	{
	case VIDCAP_LOG_NONE:
		log_level = log_level_none;
		break;
	case VIDCAP_LOG_ERROR:
		log_level = log_level_error;
		break;
	case VIDCAP_LOG_WARN:
		log_level = log_level_warn;
		break;
	case VIDCAP_LOG_INFO:
		log_level = log_level_info;
		break;
	case VIDCAP_LOG_DEBUG:
		log_level = log_level_debug;
		break;
	default:
		return -1;
	}

	log_level_set(log_level);

	return 0;
}

int
vidcap_sapi_enumerate(vidcap_state * state,
		int index,
		struct vidcap_sapi_info * sapi_info)
{
	struct vidcap_context * vcc = (struct vidcap_context *)state;
	struct sapi_context * sapi_ctx;

	if ( index < 0 || index >= vcc->num_sapi )
		return 0;

	sapi_ctx = &vcc->sapi_list[index];

	strncpy(sapi_info->identifier, sapi_ctx->identifier,
			sizeof(sapi_info->identifier));

	strncpy(sapi_info->description, sapi_ctx->description,
			sizeof(sapi_info->description));

	return 1;
}

vidcap_sapi *
vidcap_sapi_acquire(vidcap_state * state,
		const struct vidcap_sapi_info * sapi_info)
{
	struct vidcap_context * vcc = (struct vidcap_context *)state;
	struct sapi_context * sapi_ctx = 0;
	int i;

	if ( !sapi_info )
	{
		if ( vcc->num_sapi )
			sapi_ctx = &vcc->sapi_list[0];
	}
	else
	{
		for ( i = 0; i < vcc->num_sapi; ++i )
		{
			if ( strncmp(sapi_info->identifier,
						vcc->sapi_list[i].identifier,
						VIDCAP_NAME_LENGTH) == 0 )
			{
				sapi_ctx = &vcc->sapi_list[i];
				break;
			}
		}
	}

	if ( !sapi_ctx )
		goto fail;

	if ( sapi_ctx->acquire(sapi_ctx) )
		goto fail;

	return sapi_ctx;

fail:
	log_warn("unable to acquire sapi %s\n", sapi_info ?
			sapi_info->identifier :
			"default");
	return 0;
}

int
vidcap_sapi_release(vidcap_sapi * sapi)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;
	return sapi_ctx->release(sapi_ctx);
}

int
vidcap_sapi_info_get(vidcap_sapi * sapi,
		struct vidcap_sapi_info * sapi_info)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;

	strncpy(sapi_info->identifier, sapi_ctx->identifier,
			sizeof(sapi_info->identifier));

	strncpy(sapi_info->description, sapi_ctx->description,
			sizeof(sapi_info->description));

	return 0;
}

int
vidcap_srcs_notify(vidcap_sapi * sapi,
		vidcap_sapi_notify_callback callback,
		void * user_data)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;
	int ret;

	sapi_ctx->notify_callback = callback;
	sapi_ctx->notify_data = user_data;

	ret = sapi_ctx->monitor_sources(sapi_ctx);

	if ( ret )
	{
		sapi_ctx->notify_callback = 0;
		sapi_ctx->notify_data = 0;
	}

	return ret;
}

int
vidcap_src_list_update(vidcap_sapi * sapi)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;

	return sapi_ctx->scan_sources(sapi_ctx, &sapi_ctx->user_src_list);
}

int
vidcap_src_list_get(vidcap_sapi * sapi,
		int list_len,
		struct vidcap_src_info * src_list)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;

	if ( list_len < sapi_ctx->user_src_list.len )
	{
		log_error("vidcap_src_list_get: buffer size too small\n");
		return -1;
	}

	memcpy(src_list, sapi_ctx->user_src_list.list,
			sapi_ctx->user_src_list.len *
			sizeof(struct vidcap_src_info));

	return 0;
}

vidcap_src *
vidcap_src_acquire(vidcap_sapi * sapi,
		const struct vidcap_src_info * src_info)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)sapi;
	struct sapi_src_context * src_ctx = calloc(1, sizeof(*src_ctx));

	if ( !src_ctx )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	/* TODO: right now we always use the timer thread to enforce
	 *        a very accurate framerate. Alternatively, we could
	 *        allow an application to choose to forego this feature
	 *        and the associated overhead
	 */
	src_ctx->use_timer_thread = 0;

	if ( src_ctx->use_timer_thread )
	{
		src_ctx->kill_timer_thread = 0;
		src_ctx->capture_timer_thread_started = 0;
		src_ctx->capture_timer_thread = 0;
		/* FIXME: memory barrier needed? */

		if ( vc_create_thread(&src_ctx->capture_timer_thread,
					sapi_src_timer_thread_func, src_ctx,
					&src_ctx->capture_timer_thread_id) )
		{
			src_ctx->capture_timer_thread = 0;
			goto bail;
		}

		/* ensure thread has started before proceeding */
		while ( !src_ctx->capture_timer_thread_started )
			vc_millisleep(10);

		if ( src_ctx->capture_timer_thread_started < 0 )
		{
			log_error("source timer thread failed to start.\n");
			goto bail;
		}

		log_debug("using a capture timer thread for this source\n");
	}

	if ( sapi_ctx->acquire_source(sapi_ctx, src_ctx, src_info) )
	{
		log_error("failed to acquire %s\n", src_info ?
				src_info->identifier : "default");
		goto bail;
	}

	if ( sapi_src_format_list_build(src_ctx) )
	{
		log_error("failed to build format list for %s",
				src_info->identifier);
		goto bail;
	}

	src_ctx->src_state = src_acquired;
	return src_ctx;

bail:
	/* wait for it to exit (if it exists) */
	if ( src_ctx->use_timer_thread && src_ctx->capture_timer_thread )
	{
		/* signal to timer thread to stop */
		src_ctx->kill_timer_thread = 1;

		vc_thread_join(&src_ctx->capture_timer_thread);
	}

	free(src_ctx);
	return 0;
}

int
vidcap_src_info_get(vidcap_src * src,
		struct vidcap_src_info * src_info)
{
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;
	*src_info = src_ctx->src_info;
	return 0;
}

int
vidcap_src_release(vidcap_src * src)
{
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;
	int ret;

	if ( src_ctx->src_state == src_capturing )
		return -1;

	if ( src_ctx->use_timer_thread && src_ctx->capture_timer_thread )
	{
		/* signal timer thread to exit */
		src_ctx->kill_timer_thread = 1;

		/* wait for it to exit */
		vc_thread_join(&src_ctx->capture_timer_thread);
	}

	ret = src_ctx->release(src_ctx);

	if ( src_ctx->fmt_list_len )
		free(src_ctx->fmt_list);

	if ( src_ctx->fmt_conv_buf )
		free(src_ctx->fmt_conv_buf);

	if ( src_ctx->stride_free_buf )
		free(src_ctx->stride_free_buf);

	if ( src_ctx->frame_times )
		sliding_window_destroy(src_ctx->frame_times);

	free(src_ctx);

	return ret;
}

int
vidcap_format_enumerate(vidcap_src * src,
		int index,
		struct vidcap_fmt_info * fmt_info)
{
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;

	if ( index < 0 || index >= src_ctx->fmt_list_len )
		return 0;

	*fmt_info = src_ctx->fmt_list[index];
	return 1;
}

int
vidcap_format_bind(vidcap_src * src,
		const struct vidcap_fmt_info * fmt_info)
{
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;
	struct vidcap_fmt_info fmt_native;

	if ( src_ctx->src_state == src_capturing )
		return -1;

	if ( !fmt_info )
	{
		if ( !src_ctx->fmt_list_len )
		{
			log_warn("no default format for %s\n",
					src_ctx->src_info.identifier);
			return -1;
		}

		fmt_info = &src_ctx->fmt_list[0];
	}

	if ( !src_ctx->format_validate(src_ctx, fmt_info, &fmt_native) )
	{
		log_error("format not supported by source: %dx%d %s %d/%d fps\n",
				fmt_info->width, fmt_info->height,
				vidcap_fourcc_string_get(fmt_info->fourcc),
				fmt_info->fps_numerator,
				fmt_info->fps_denominator);
		return -1;
	}

	if ( src_ctx->format_bind(src_ctx, fmt_info) )
		return -1;

	src_ctx->fmt_native = fmt_native;
	src_ctx->fmt_nominal = *fmt_info;

	src_ctx->fmt_conv_func = conv_conversion_func_get(
			src_ctx->fmt_native.fourcc,
			src_ctx->fmt_nominal.fourcc);

	if ( src_ctx->stride_free_buf )
	{
		free(src_ctx->stride_free_buf);
		src_ctx->stride_free_buf = 0;
	}

	src_ctx->stride_free_buf_size = conv_fmt_size_get(
				src_ctx->fmt_native.width,
				src_ctx->fmt_native.height,
				src_ctx->fmt_native.fourcc);

	if ( !src_ctx->stride_free_buf_size )
	{
		log_error("failed to get stride-free buffer size for %s\n",
				vidcap_fourcc_string_get(
						src_ctx->fmt_native.fourcc));
		return -1;
	}

	if ( !(src_ctx->stride_free_buf =
				malloc(src_ctx->stride_free_buf_size)) )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	if ( src_ctx->fmt_conv_buf )
	{
		free(src_ctx->fmt_conv_buf);
		src_ctx->fmt_conv_buf = 0;
	}

	if ( src_ctx->fmt_conv_func )
	{
		src_ctx->fmt_conv_buf_size = conv_fmt_size_get(
					src_ctx->fmt_nominal.width,
					src_ctx->fmt_nominal.height,
					src_ctx->fmt_nominal.fourcc);

		if ( !src_ctx->fmt_conv_buf_size )
		{
			log_error("failed to get buffer size for %s\n",
					vidcap_fourcc_string_get(
						fmt_info->fourcc));
			return -1;
		}

		if ( !(src_ctx->fmt_conv_buf =
					malloc(src_ctx->fmt_conv_buf_size)) )
		{
			log_oom(__FILE__, __LINE__);
			return -1;
		}

		log_debug("format bind requires conversion: %s\n",
				conv_conversion_name_get(src_ctx->fmt_conv_func));
	}

	src_ctx->src_state = src_bound;

	return 0;
}

int
vidcap_format_info_get(vidcap_src * src,
		struct vidcap_fmt_info * fmt_info)
{
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;
	*fmt_info = src_ctx->fmt_nominal;
	return 0;
}

static __inline void
copy_frame_info(void * fr2, const void *fr1)
{
	const struct frame_info *frame_orig = (struct frame_info *)fr1;
	struct frame_info *frame_dup = (struct frame_info *)fr2;

	frame_dup->capture_time    = frame_orig->capture_time;
	frame_dup->error_status    = frame_orig->error_status;
	frame_dup->stride          = frame_orig->stride;
	frame_dup->video_data_size = frame_orig->video_data_size;

	memcpy(frame_dup->video_data,
			frame_orig->video_data,
			frame_orig->video_data_size);
}

int
vidcap_src_capture_start(vidcap_src * src,
		vidcap_src_capture_callback callback,
		void * user_data)
{
	int ret = 0;
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;
	const int sliding_window_seconds = 4;

	/* Assume worst-case video data with stride is 50%
	 * bigger than without a stride.
	 */
	/* FIXME: Defer measurement of stride-full 
	 *        buffer until first capture callback
	 */
	const int stride_full_buf_size = src_ctx->stride_free_buf_size * 3 / 2;

	if ( user_data == VIDCAP_INVALID_USER_DATA )
		return -3;

	if ( src_ctx->src_state != src_bound )
		return -4;

	memset(src_ctx->buffered_frames, 0, sizeof(src_ctx->buffered_frames));
	src_ctx->timer_thread_frame.video_data = 0;
	src_ctx->frame_times = 0;
	src_ctx->double_buff = 0;

	if ( src_ctx->use_timer_thread )
	{
		/* allocate space for double-buffering video buffers */
		src_ctx->buffered_frames[0].video_data =
				malloc(2 * stride_full_buf_size);
		if ( !src_ctx->buffered_frames[0].video_data )
		{
			log_oom(__FILE__, __LINE__);
			ret = -6;
			goto capture_start_bail;
		}

		src_ctx->buffered_frames[1].video_data =
				src_ctx->buffered_frames[0].video_data +
						stride_full_buf_size;

		src_ctx->timer_thread_frame.video_data =
				malloc(stride_full_buf_size);
		if ( !src_ctx->timer_thread_frame.video_data )
		{
			log_oom(__FILE__, __LINE__);
			ret = -6;
			goto capture_start_bail;
		}

		src_ctx->double_buff = double_buffer_create(&copy_frame_info,
				&src_ctx->buffered_frames[0], &src_ctx->buffered_frames[1]);
		if ( !src_ctx->double_buff )
		{
			ret = -5;
			goto capture_start_bail;
		}
	}
	else
	{
		src_ctx->frame_time_next.tv_sec = 0;
		src_ctx->frame_time_next.tv_usec = 0;
		src_ctx->frame_times = sliding_window_create(sliding_window_seconds *
				src_ctx->fmt_nominal.fps_numerator / 
				src_ctx->fmt_nominal.fps_denominator,
				sizeof(struct timeval));

		if ( !src_ctx->frame_times )
			return -2;
	}

	src_ctx->capture_callback = callback;
	src_ctx->capture_data = user_data;

	if ( (ret = src_ctx->start_capture(src_ctx)) )
		goto capture_start_bail;

	src_ctx->src_state = src_capturing;
	return 0;

capture_start_bail:

	src_ctx->capture_callback = 0;
	src_ctx->capture_data = VIDCAP_INVALID_USER_DATA;

	if ( src_ctx->double_buff )
		double_buffer_destroy(src_ctx->double_buff);

	if ( src_ctx->buffered_frames[0].video_data )
		free(src_ctx->buffered_frames[0].video_data);

	if ( src_ctx->buffered_frames[1].video_data )
		free(src_ctx->buffered_frames[1].video_data);

	if ( src_ctx->timer_thread_frame.video_data )
		free(src_ctx->timer_thread_frame.video_data);

	if ( src_ctx->frame_times )
		sliding_window_destroy(src_ctx->frame_times);

	src_ctx->double_buff = 0;
	src_ctx->frame_times = 0;

	return ret;
}

int
vidcap_src_capture_stop(vidcap_src * src)
{
	int ret;
	struct sapi_src_context * src_ctx = (struct sapi_src_context *)src;

	if ( src_ctx->src_state != src_capturing )
		return -1;

	ret = src_ctx->stop_capture(src_ctx);

	/* signal to timer thread that capture has stopped */
	src_ctx->src_state = src_bound;

	if ( src_ctx->use_timer_thread )
	{
		/* FIXME: memory barrier needed? */
		sapi_src_timer_thread_idled(src_ctx);
	}

	if ( src_ctx->double_buff )
		double_buffer_destroy(src_ctx->double_buff);
	src_ctx->double_buff = 0;

	if ( src_ctx->buffered_frames[0].video_data )
		free(src_ctx->buffered_frames[0].video_data);

	if ( src_ctx->buffered_frames[1].video_data )
		free(src_ctx->buffered_frames[1].video_data);

	if ( src_ctx->timer_thread_frame.video_data )
		free(src_ctx->timer_thread_frame.video_data);

	if ( src_ctx->frame_times )
		sliding_window_destroy(src_ctx->frame_times);
	src_ctx->frame_times = 0;

	src_ctx->capture_callback = 0;
	src_ctx->capture_data = VIDCAP_INVALID_USER_DATA;

	return ret;
}

const char *
vidcap_fourcc_string_get(int fourcc)
{
	switch ( fourcc )
	{
		case VIDCAP_FOURCC_I420:
			return "i420";
		case VIDCAP_FOURCC_YUY2:
			return "yuy2";
		case VIDCAP_FOURCC_RGB32:
			return "rgb32";
		case VIDCAP_FOURCC_RGB24:
			return "rgb24";
		case VIDCAP_FOURCC_BOTTOM_UP_RGB24:
			return "bottom_up_rgb24";
		case VIDCAP_FOURCC_RGB555:
			return "rgb555";
		case VIDCAP_FOURCC_YVU9:
			return "yvu9";
		case VIDCAP_FOURCC_2VUY:
			return "2vuy";
		default:
			return "????";
	}
}
