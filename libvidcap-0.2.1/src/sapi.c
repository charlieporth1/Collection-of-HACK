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

#include "hotlist.h"
#include "logging.h"
#include "sapi.h"

static __inline int
tv_greater_or_equal(struct timeval * t1, struct timeval * t0)
{
	if ( t1->tv_sec > t0->tv_sec)
		return 1;

	if ( t1->tv_sec < t0->tv_sec)
		return 0;

	if ( t1->tv_usec >= t0->tv_usec )
		return 1;

	return 0;
}

static __inline void
tv_add_usecs(struct timeval *t1, struct timeval *t0, long usecs)
{
	int secs_carried = 0;

	t1->tv_usec = t0->tv_usec + usecs;
	secs_carried = t1->tv_usec / 1000000;
	t1->tv_usec %= 1000000;

	t1->tv_sec = t0->tv_sec + secs_carried;
}

static int
enforce_framerate(struct sapi_src_context * src_ctx)
{
	struct timeval  tv_earliest_next;
	struct timeval  tv_now;
	struct timeval *tv_oldest = 0;
	int first_time = 0;

	if ( !src_ctx->frame_times )
		return 0;

	first_time = !sliding_window_count(src_ctx->frame_times);

	tv_now = vc_now();

	if ( !first_time && !tv_greater_or_equal(
				&tv_now, &src_ctx->frame_time_next) )
		return 0;

	/* Allow frame through */

	tv_oldest = sliding_window_peek_front(src_ctx->frame_times);

	if ( !tv_oldest )
		tv_oldest = &tv_now;

	/* calculate next frame time based on sliding window */
	tv_add_usecs(&src_ctx->frame_time_next, tv_oldest,
			sliding_window_count(src_ctx->frame_times) *
			1000000 *
			src_ctx->fmt_nominal.fps_denominator /
			src_ctx->fmt_nominal.fps_numerator);

	/* calculate the earliest allowable next frame time
	 * based on: now + arbitrary minimum spacing
	 */
	tv_add_usecs(&tv_earliest_next, &tv_now, 900000 *
			src_ctx->fmt_nominal.fps_denominator /
			src_ctx->fmt_nominal.fps_numerator);

	/* if earliest allowable > next, next = earliest */
	/* if there's been a stall, pace ourselves       */
	if ( tv_greater_or_equal(&tv_earliest_next,
				&src_ctx->frame_time_next) )
		src_ctx->frame_time_next = tv_earliest_next;

	sliding_window_slide(src_ctx->frame_times, &tv_now);

	return 1;
}

int
sapi_acquire(struct sapi_context * sapi_ctx)
{
	if ( sapi_ctx->ref_count )
		return -1;

	sapi_ctx->ref_count++;
	return 0;
}

int
sapi_release(struct sapi_context * sapi_ctx)
{
	if ( !sapi_ctx->ref_count )
		return -1;

	sapi_ctx->ref_count--;
	return 0;
}

int
sapi_src_format_list_build(struct sapi_src_context * src_ctx)
{
	struct vidcap_fmt_info fmt_info;
	struct vidcap_fmt_info * list = 0;
	int list_len = 0;
	int i, j, k;

	if ( src_ctx->fmt_list )
	{
		log_error("source alread has format list\n");
		return -1;
	}

	for ( i = 0; i < hot_fourcc_list_len; ++i )
	{
		fmt_info.fourcc = hot_fourcc_list[i];

		for ( j = 0; j < hot_fps_list_len; ++j )
		{
			fmt_info.fps_numerator =
				hot_fps_list[j].fps_numerator;
			fmt_info.fps_denominator =
				hot_fps_list[j].fps_denominator;

			for ( k = 0; k < hot_resolution_list_len; ++k )
			{
				struct vidcap_fmt_info fmt_native;

				fmt_info.width = hot_resolution_list[k].width;
				fmt_info.height = hot_resolution_list[k].height;

				if ( !src_ctx->format_validate(src_ctx,
							&fmt_info,
							&fmt_native) )
					continue;

				list = realloc(list,
						++list_len * sizeof(fmt_info));

				if ( !list )
				{
					log_oom(__FILE__, __LINE__);
					return -1;
				}

				list[list_len-1] = fmt_info;
			}
		}
	}

	src_ctx->fmt_list = list;
	src_ctx->fmt_list_len = list_len;

	return 0;
}

static void
wait_for_error_ack(struct sapi_src_context * src_ctx)
{
	while ( !src_ctx->capture_error_ack )
		vc_millisleep(10);
}

static void
acknowledge_error(struct sapi_src_context * src_ctx)
{
	src_ctx->capture_error_ack = 1;
}

static int
deliver_frame(struct sapi_src_context * src_ctx)
{
	vidcap_src_capture_callback cap_callback = src_ctx->capture_callback;
	void * cap_data = src_ctx->capture_data;
	void * buf = 0;
	int buf_data_size = 0;
	int send_frame = 0;

	struct vidcap_capture_info cap_info;

	const struct frame_info * frame;
	const char * video_data;
	int video_data_size;
	int stride;
	int error_status;

	if ( src_ctx->use_timer_thread )
	{
		if ( double_buffer_read(src_ctx->double_buff,
						&src_ctx->timer_thread_frame) )
			return -1;

		frame = &src_ctx->timer_thread_frame;
	}
	else
	{
		frame = &src_ctx->callback_frame;
	}

	video_data      = frame->video_data;
	video_data_size = frame->video_data_size;
	stride          = frame->stride;
	error_status    = frame->error_status;
	cap_info.capture_time_sec  = frame->capture_time.tv_sec;
	cap_info.capture_time_usec = frame->capture_time.tv_usec;

	/* FIXME: right now enforce_framerate() and the capture timer
	 *        thread's main loop BOTH use the frame_time_next field
	 */
	if ( src_ctx->use_timer_thread )
	{
		send_frame = 1;
	}
	else
	{
		if ( !error_status )
			send_frame = enforce_framerate(src_ctx);

		if ( send_frame < 0 )
			error_status = -1000;
	}

	cap_info.error_status = error_status;

	if ( !cap_info.error_status && stride &&
			!destridify(src_ctx->fmt_native.width,
				src_ctx->fmt_native.height,
				src_ctx->fmt_native.fourcc,
				stride,
				video_data,
				src_ctx->stride_free_buf) )
	{
		buf = src_ctx->stride_free_buf;
		buf_data_size = src_ctx->stride_free_buf_size;
	}
	else
	{
		buf = (void *)video_data;
		buf_data_size = video_data_size;
	}

	if ( cap_info.error_status )
	{
		cap_info.video_data = 0;
		cap_info.video_data_size = 0;
	}
	else if ( src_ctx->fmt_conv_func )
	{
		if ( src_ctx->fmt_conv_func(
					src_ctx->fmt_native.width,
					src_ctx->fmt_native.height,
					buf,
					src_ctx->fmt_conv_buf) )
		{
			log_error("failed format conversion\n");
			cap_info.error_status = -1;
		}

		cap_info.video_data = src_ctx->fmt_conv_buf;
		cap_info.video_data_size = src_ctx->fmt_conv_buf_size;
	}
	else
	{
		cap_info.video_data = buf;
		cap_info.video_data_size = buf_data_size;
	}

	if ( ( send_frame || error_status ) && cap_callback &&
			cap_data != VIDCAP_INVALID_USER_DATA )
	{
		/* FIXME: Need to check return code (and pass it back).
		 *        Application may want capture to stop.
		 *        Ensure we don't perform any more callbacks.
		 */
		cap_callback(src_ctx, cap_data, &cap_info);

		if ( src_ctx->use_timer_thread )
		{
			if ( error_status )
			{
				/* Let capture thread know that the app
				 * Has received the error status.
				 * Ensure we don't deliver any more frames.
				 */
				acknowledge_error(src_ctx);

				/* inform calling function of error */
				return 1;
			}
		}
	}

	return 0;
}

/* NOTE: stride-ignorant sapis should pass a stride of zero */
int
sapi_src_capture_notify(struct sapi_src_context * src_ctx,
		char * video_data, int video_data_size,
		int stride,
		int error_status)
{
	/* NOTE: We may be called here by the capture thread while the
	 * main thread is clearing capture_data and capture_callback
	 * from within vidcap_src_capture_stop().
	 */

	struct frame_info *frame = &src_ctx->callback_frame;

	/* Screen-out useless callbacks */
	if ( video_data_size < 1 && !error_status )
	{
		log_info("callback with no data?\n");
		return 0;
	}

	/* Package the video information */
	frame->video_data_size = video_data_size;
	frame->error_status = error_status;
	frame->stride = stride;
	frame->capture_time = vc_now();
	frame->video_data = video_data;

	if ( src_ctx->use_timer_thread )
	{
		/* buffer the frame - for processing by the timer thread */
		double_buffer_write(src_ctx->double_buff, frame);

		/* If there's an error, wait here until it's acknowledged
		 * by the timer thread (after having delivered it to the app).
		 */
		if ( error_status )
			wait_for_error_ack(src_ctx);
	}
	else
	{
		/* process the frame now */
		deliver_frame(src_ctx);
	}

	if ( error_status )
	{
		src_ctx->src_state = src_bound;
		src_ctx->capture_callback = 0;
		src_ctx->capture_data = VIDCAP_INVALID_USER_DATA;
	}

	return 0;
}

unsigned int
STDCALL sapi_src_timer_thread_func(void *args)
{
	struct sapi_src_context * src_ctx = args;
	struct timeval tv_now;
	const long idle_state_sleep_period_ms = 100;
	long sleep_ms = idle_state_sleep_period_ms;
	int first_time = 1;
	int got_frame = 0;
	int ret;
	int capture_error = 0;    /* FIXME: perhaps should exit on error */

	src_ctx->timer_thread_idle = 1;

	tv_now = vc_now();

	src_ctx->frame_time_next.tv_sec = tv_now.tv_sec;
	src_ctx->frame_time_next.tv_usec = tv_now.tv_usec;

	src_ctx->capture_timer_thread_started = 1;

	while ( !src_ctx->kill_timer_thread )
	{
		tv_now = vc_now();

		/* sleep or read? */
		if ( capture_error || src_ctx->src_state != src_capturing ||
				!tv_greater_or_equal(&tv_now, &src_ctx->frame_time_next) )
		{
			if ( src_ctx->src_state != src_capturing )
			{
				sleep_ms = idle_state_sleep_period_ms;
				first_time = 1;
			}
			else if ( !capture_error )
			{
				/* sleep just enough */
				sleep_ms = ((src_ctx->frame_time_next.tv_sec - tv_now.tv_sec) *
						1000000L + src_ctx->frame_time_next.tv_usec -
						tv_now.tv_usec) / 1000L;
			}

			if ( sleep_ms < 0 )
				sleep_ms = 0;

			vc_millisleep(sleep_ms);
		}
		else
		{
			src_ctx->timer_thread_idle = 0;
			/* FIXME: memory barrier needed? */

			/* attempt to read and deliver a frame */
			ret = deliver_frame(src_ctx);

			got_frame = !ret;
			capture_error = ret > 0;

			/* Is this the first frame? */
			if ( got_frame && first_time )
			{
				first_time = 0;

				/* re-initialize when next to check for a frame */
				src_ctx->frame_time_next.tv_sec = tv_now.tv_sec;
				src_ctx->frame_time_next.tv_usec = tv_now.tv_usec;
			}

			if ( !first_time )
			{
				/* update when next to check for a frame */
				tv_add_usecs(&src_ctx->frame_time_next, &src_ctx->frame_time_next,
						1000000 *
						src_ctx->fmt_nominal.fps_denominator /
						src_ctx->fmt_nominal.fps_numerator);
			}
			else
			{
				/* still no first frame */
				/* update when next to check for a frame */
				tv_add_usecs(&src_ctx->frame_time_next, &tv_now,
						1000000 *
						src_ctx->fmt_nominal.fps_denominator /
						src_ctx->fmt_nominal.fps_numerator);
			}
		}

		/* FIXME: memory barrier needed? */
		src_ctx->timer_thread_idle = 1;
	}

	return 0;
}

void
sapi_src_timer_thread_idled(struct sapi_src_context * src_ctx)
{
	while ( !src_ctx->timer_thread_idle )
		vc_millisleep(10);
}

int
sapi_can_convert_native_to_nominal(const struct vidcap_fmt_info * fmt_native,
		const struct vidcap_fmt_info * fmt_nominal)
{
	const float native_fps =
		(float)fmt_native->fps_numerator /
		(float)fmt_native->fps_denominator;

	const float nominal_fps =
		(float)fmt_nominal->fps_numerator /
		(float)fmt_nominal->fps_denominator;

	if ( native_fps < nominal_fps )
		return 0;

	if ( fmt_native->width != fmt_nominal->width ||
			fmt_native->height != fmt_nominal->height )
		return 0;

	if ( fmt_native->fourcc == fmt_nominal->fourcc )
		return 1;

	if ( conv_conversion_func_get(fmt_native->fourcc, fmt_nominal->fourcc) )
		return 1;

	return 0;
}

