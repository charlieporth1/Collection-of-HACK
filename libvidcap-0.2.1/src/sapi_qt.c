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
#include <strings.h>

#include <QuickTime/QuickTime.h>

#include "conv.h"
#include "hotlist.h"
#include "logging.h"
#include "sapi.h"
#include "quicktime/sg_manager.h"

static const char * identifier = "quicktime";
static const char * description = "Apple QuickTime";

struct sapi_qt_context
{
	vc_mutex mutex;
	vc_thread notify_thread;
	unsigned int notify_thread_id;
	int notifying;

	struct sg_manager mgr;
};

struct sapi_qt_src_context
{
	struct sg_source * src;

	int capturing;
	vc_thread capture_thread;
	unsigned int capture_thread_id;

	TimeValue last_time;
	long frame_count;

	ICMDecompressionSessionRef decomp_session;
};

static int
mutex_lock(vc_mutex * mutex)
{
	int err = vc_mutex_lock(mutex);
	if ( err )
		log_error("failed vc_mutex_lock() %d\n", err);
	return err;
}

static int
mutex_unlock(vc_mutex * mutex)
{
	int err = vc_mutex_unlock(mutex);
	if ( err )
		log_error("failed vc_mutex_unlock() %d\n", err);
	return err;
}

static int
map_ostype_to_fourcc(OSType pixel_format, int * fourcc)
{
	switch ( pixel_format )
	{
	case k32BGRAPixelFormat:
		*fourcc = VIDCAP_FOURCC_RGB32;
		break;
	case kYUVSPixelFormat:
		*fourcc = VIDCAP_FOURCC_YUY2;
		break;
	case kOpenDMLJPEGCodecType: /* 'dmb1' */
	case k2vuyPixelFormat:
		*fourcc = VIDCAP_FOURCC_2VUY;
		break;

	case k16LE555PixelFormat:
		*fourcc = VIDCAP_FOURCC_RGB555;
		return 1;
	case k24RGBPixelFormat:
		*fourcc = VIDCAP_FOURCC_RGB24;
		return 1;
	case kYVU9PixelFormat:
		*fourcc = VIDCAP_FOURCC_YVU9;
		return 1;
	case kYUV420PixelFormat:
		*fourcc = VIDCAP_FOURCC_I420;
		return 1;

	default:
		return -1;
	}

	return 0;
}

static int
map_fourcc_to_ostype(int fourcc, OSType * pixel_format)
{
	switch ( fourcc )
	{
	case VIDCAP_FOURCC_RGB32:
		*pixel_format = k32BGRAPixelFormat;
		break;
	case VIDCAP_FOURCC_YUY2:
		*pixel_format = kYUVSPixelFormat;
		break;
	case VIDCAP_FOURCC_2VUY:
		*pixel_format = k2vuyPixelFormat;
		break;

	case VIDCAP_FOURCC_RGB555:
		*pixel_format = k16LE555PixelFormat;
		return 1;
	case VIDCAP_FOURCC_RGB24:
		*pixel_format = k24RGBPixelFormat;
		return 1;
	case VIDCAP_FOURCC_YVU9:
		*pixel_format = kYVU9PixelFormat;
		return 1;
	case VIDCAP_FOURCC_I420:
		*pixel_format = kYUV420PixelFormat;
		return 1;

	default:
		return -1;
	}

	return 0;
}

static int
src_info_from_src(struct vidcap_src_info * src_info,
		const struct sg_source * src)
{
	snprintf(src_info->identifier, sizeof(src_info->identifier),
			"%d,%d", src->device_id, src->input_id);

	snprintf(src_info->description, sizeof(src_info->description),
			"%.*s - %.*s",
			src->input_name[0], &src->input_name[1],
			src->device_name[0], &src->device_name[1]);

	return 0;
}

static void
source_decomp_callback(void * decomp_tracking_refcon,
		OSStatus result,
		ICMDecompressionTrackingFlags decomp_tracking_flags,
		CVPixelBufferRef pixel_buffer,
		TimeValue64 display_time,
		TimeValue64 display_duration,
		ICMValidTimeFlags valid_time_flags,
		void * reserved,
		void * source_frame_refcon)
{
	CVReturn err;
	struct sapi_src_context * src_ctx =
		(struct sapi_src_context *)decomp_tracking_refcon;
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;
	OSType pixel_format;

	if ( result != noErr )
	{
		if ( result == codecBadDataErr )
			log_warn("source_decomp_callback codecBadDataErr\n");
		else
			/* TODO: In this case, we may want to stop capture */
			log_warn("source_decomp_callback failure %ld\n",
				result);
		return;
	}

	pixel_format = CVPixelBufferGetPixelFormatType(pixel_buffer);

	if ( (err = CVPixelBufferLockBaseAddress(pixel_buffer, 0)) )
	{
		log_error("failed CVPixelBufferLockBaseAddress() %d\n", err);
		return;
	}

	if ( qt_src_ctx->frame_count == 1 )
		log_debug("capture time: %c%c%c%c  %s  %s  %s\n",
				(char)(pixel_format >> 24),
				(char)(pixel_format >> 16),
				(char)(pixel_format >> 8),
				(char)(pixel_format >> 0),
				vidcap_fourcc_string_get(
					src_ctx->fmt_native.fourcc),
				vidcap_fourcc_string_get(
					src_ctx->fmt_nominal.fourcc),
				src_ctx->src_info.identifier);

	sapi_src_capture_notify(src_ctx,
			CVPixelBufferGetBaseAddress(pixel_buffer),
			CVPixelBufferGetDataSize(pixel_buffer),
			CVPixelBufferGetBytesPerRow(pixel_buffer), 0);

	if ( (err = CVPixelBufferUnlockBaseAddress(pixel_buffer, 0)) )
	{
		log_error("failed CVPixelBufferUnlockBaseAddress() %d\n", err);
		return;
	}
}

static OSErr
source_decomp_session_takedown(struct sapi_src_context * src_ctx)
{
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	if ( qt_src_ctx->decomp_session )
	{
		ICMDecompressionSessionFlush(qt_src_ctx->decomp_session);
		ICMDecompressionSessionRelease(qt_src_ctx->decomp_session);
		qt_src_ctx->decomp_session = 0;
	}

	return noErr;
}

static OSErr
source_decomp_session_setup(struct sapi_src_context * src_ctx)
{
	OSErr err;
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;
	ImageDescriptionHandle image_desc = (ImageDescriptionHandle)NewHandle(0);
	ICMDecompressionTrackingCallbackRecord tracking_callback_record;
	CFMutableDictionaryRef pixel_buffer_attribs;
	CFNumberRef n;
	OSType pixel_format;

	tracking_callback_record.decompressionTrackingCallback =
		source_decomp_callback;
	tracking_callback_record.decompressionTrackingRefCon = src_ctx;

	if ( sg_source_image_desc_get(qt_src_ctx->src, image_desc) )
		return -1;

	pixel_buffer_attribs = CFDictionaryCreateMutable(0, 0,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

	CFDictionaryAddValue(pixel_buffer_attribs,
			kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);

	n = CFNumberCreate(0, kCFNumberSInt32Type, &src_ctx->fmt_native.width);
	CFDictionaryAddValue(pixel_buffer_attribs, kCVPixelBufferWidthKey, n);
	CFRelease(n);

	n = CFNumberCreate(0, kCFNumberSInt32Type, &src_ctx->fmt_native.height);
	CFDictionaryAddValue(pixel_buffer_attribs, kCVPixelBufferHeightKey, n);
	CFRelease(n);

	/* Here's our big chance to set the pixel buffer format to do
	 * whatever the user wants. QuickTime will do it's magic and
	 * we ride off into the sunset, victorious.
	 */

	if ( map_fourcc_to_ostype(src_ctx->fmt_native.fourcc, &pixel_format) )
	{
		log_error("invalid bound fourcc '%s'\n",
				vidcap_fourcc_string_get(
					src_ctx->fmt_native.fourcc));
		return -1;
	}

	log_debug("setup decomp: %c%c%c%c  %s  %s  %s\n",
			(char)(pixel_format >> 24),
			(char)(pixel_format >> 16),
			(char)(pixel_format >> 8),
			(char)(pixel_format >> 0),
			vidcap_fourcc_string_get(src_ctx->fmt_native.fourcc),
			vidcap_fourcc_string_get(src_ctx->fmt_nominal.fourcc),
			src_ctx->src_info.identifier);

	n = CFNumberCreate(0, kCFNumberSInt32Type, &pixel_format);
	CFDictionaryAddValue(pixel_buffer_attribs,
			kCVPixelBufferPixelFormatTypeKey, n);
	CFRelease(n);

	err = ICMDecompressionSessionCreate(
			0, /* default allocator */
			image_desc,
			0, /* session options */
			(CFDictionaryRef)pixel_buffer_attribs,
			&tracking_callback_record,
			&qt_src_ctx->decomp_session);

	CFRelease(pixel_buffer_attribs);
	DisposeHandle((Handle)image_desc);

	if ( err )
		log_error("failed ICMDecompressionSessionCreate() %d\n", err);

	return err;
}

static OSErr
source_capture_callback(SGChannel channel, Ptr data, long data_len,
		long * offset, long channel_ref_con, TimeValue time_value,
		short write_type, long ref_con)
{
	OSErr err;
	struct sapi_src_context * src_ctx =
		(struct sapi_src_context *)ref_con;
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	ICMFrameTimeRecord frame_time;

	frame_time.recordSize = sizeof(ICMFrameTimeRecord);
	*(TimeValue64 *)&frame_time.value = time_value;
	frame_time.scale = qt_src_ctx->src->time_scale;
	frame_time.rate = fixed1;
	frame_time.frameNumber = ++qt_src_ctx->frame_count;
	frame_time.flags = icmFrameTimeIsNonScheduledDisplayTime;

	err = ICMDecompressionSessionDecodeFrame(qt_src_ctx->decomp_session,
			(const UInt8 *)data, data_len, 0, &frame_time, src_ctx);

	ICMDecompressionSessionSetNonScheduledDisplayTime(qt_src_ctx->decomp_session,
			time_value, qt_src_ctx->src->time_scale, 0);

	qt_src_ctx->last_time = time_value;

	return noErr;
}

static struct timespec
capture_poll_timespec(double fps)
{
	const double fudge_factor = 0.5;
	const long ns_per_ms = 1000000;
	const double ns_per_s = 1000000000.0;
	const long wait_ns = (long)(fudge_factor * ns_per_s / fps);

	const long min_wait_ns = 5 * ns_per_ms;
	const long max_wait_ns = 10 * ns_per_ms;

	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = wait_ns < min_wait_ns ?
		min_wait_ns :
		(wait_ns > max_wait_ns ? max_wait_ns :
		 wait_ns);

	return ts;
}

static unsigned int
capture_thread_proc(void * data)
{
	struct sapi_src_context * src_ctx =
		(struct sapi_src_context *)data;
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	const double fps =
		(double)src_ctx->fmt_native.fps_numerator /
		(double)src_ctx->fmt_native.fps_denominator;

	const struct timespec wait_ts = capture_poll_timespec(fps);

	int ret = 0;

	OSType pixel_format;

	if ( map_fourcc_to_ostype(src_ctx->fmt_native.fourcc, &pixel_format) )
	{
		log_error("invalid pixel format '%s' (0x%0x)\n",
				vidcap_fourcc_string_get(
					src_ctx->fmt_native.fourcc),
				src_ctx->fmt_native.fourcc);

		ret = -2;
		sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
		return ret;
	}

	if ( sg_source_format_set(qt_src_ctx->src,
				src_ctx->fmt_native.width,
				src_ctx->fmt_native.height,
				fps, pixel_format) )
	{
		ret = -3;
		sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
		return ret;
	}

	if ( qt_src_ctx->decomp_session )
	{
		log_error("decomp session already setup\n");
		ret = -4;
		sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
		return ret;
	}

	qt_src_ctx->last_time = 0;
	qt_src_ctx->frame_count = 0;

	if ( sg_source_capture_start(qt_src_ctx->src, source_capture_callback,
				(long)src_ctx) )
	{
		ret = -5;
		sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
		return ret;
	}

	if ( source_decomp_session_setup(src_ctx) )
	{
		ret = -6;
		sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
		return ret;
	}

	while ( qt_src_ctx->capturing )
	{
		struct timespec remain_ts;

		if ( sg_source_capture_poll(qt_src_ctx->src) )
		{
			ret = -7;
			sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
			goto bail;
		}

		if ( nanosleep(&wait_ts, &remain_ts) == -1 )
		{
			if ( errno != -EINTR )
			{
				log_error("failed nanosleep() %d\n", errno);
				ret = -8;
				sapi_src_capture_notify(src_ctx, 0, 0, 0, ret);
				goto bail;
			}
		}
	}

bail:

	if ( source_decomp_session_takedown(src_ctx) )
		ret -= 100;

	if ( sg_source_capture_stop(qt_src_ctx->src) )
		ret -= -1000;

	return ret;
}

static int
source_capture_start(struct sapi_src_context * src_ctx)
{
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;
	int ret;

	qt_src_ctx->capturing = 1;

	if ( (ret = vc_create_thread(&qt_src_ctx->capture_thread,
			capture_thread_proc,
			src_ctx, &qt_src_ctx->capture_thread_id)) )
	{
		log_error("failed vc_thread_create() for capture thread %d\n",
				ret);
		return -1;
	}

	return 0;
}

static int
source_capture_stop(struct sapi_src_context * src_ctx)
{
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	int ret;

	qt_src_ctx->capturing = 0;

	if ( (ret = vc_thread_join(&qt_src_ctx->capture_thread)) )
	{
		log_error("failed vc_thread_join() %d\n", ret);
		return -1;
	}

	return 0;
}

static int
source_format_validate(struct sapi_src_context * src_ctx,
		const struct vidcap_fmt_info * fmt_nominal,
		struct vidcap_fmt_info * fmt_native)
{
	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	*fmt_native = *fmt_nominal;

	if ( map_ostype_to_fourcc(qt_src_ctx->src->native_pixel_format,
				&fmt_native->fourcc) )
		/* We can always squeeze RGB32 out of quicktime */
		fmt_native->fourcc = VIDCAP_FOURCC_RGB32;

	if ( !sg_source_format_validate(qt_src_ctx->src,
				fmt_native->width,
				fmt_native->height,
				(float)fmt_native->fps_numerator /
				(float)fmt_native->fps_denominator) )
		return 0;

	if ( !sapi_can_convert_native_to_nominal(fmt_native, fmt_nominal) )
		return 0;

	return 1;
}

static int
source_format_bind(struct sapi_src_context * src_ctx,
		const struct vidcap_fmt_info * fmt_info)
{
	return 0;
}

static int
parse_src_identifier(const char * identifier, int * device_id, int * input_id)
{
	return sscanf(identifier, "%d,%d", device_id, input_id) == 2 ?
		0 : -1;
}

static int
scan_sources(struct sapi_context * sapi_ctx, struct sapi_src_list * src_list)
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;
	struct sg_source * src;
	int src_index = 0;
	int i;

	if ( mutex_lock(&qt_ctx->mutex) )
		return -1;

	if ( sg_manager_update(&qt_ctx->mgr) )
	{
		mutex_unlock(&qt_ctx->mutex);
		return -1;
	}

	src_list->len = sg_manager_source_count(&qt_ctx->mgr);
	src_list->list = calloc(src_list->len, sizeof(struct vidcap_src_info));

	if ( !src_list->list )
	{
		log_oom(__FILE__, __LINE__);
		mutex_unlock(&qt_ctx->mutex);
		return -1;
	}

	for ( i = 0; (src = sg_manager_source_get(&qt_ctx->mgr, i)); ++i )
		src_info_from_src(&src_list->list[src_index++], src);

	if ( mutex_unlock(&qt_ctx->mutex) )
		return -1;

	return src_list->len;
}

static int
source_release(struct sapi_src_context * src_ctx)
{
	int ret = 0;

	struct sapi_qt_src_context * qt_src_ctx =
		(struct sapi_qt_src_context *)src_ctx->priv;

	if ( qt_src_ctx->capturing )
		ret = source_capture_stop(src_ctx);

	sg_source_release(qt_src_ctx->src);

	free(qt_src_ctx);
	return ret;
}

static int
source_acquire(struct sapi_context * sapi_ctx,
		struct sapi_src_context * src_ctx,
		const struct vidcap_src_info * src_info)
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;
	struct sapi_qt_src_context * qt_src_ctx;

	if ( scan_sources(sapi_ctx, &sapi_ctx->user_src_list) <= 0 )
		return -1;

	if ( !(qt_src_ctx = calloc(1, sizeof(*qt_src_ctx))) )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	if ( mutex_lock(&qt_ctx->mutex) )
		return -1;

	if ( src_info )
	{
		int device_id;
		int input_id;

		if ( parse_src_identifier(src_info->identifier, &device_id,
					&input_id) )
		{
			log_error("invalid source identifier (%s)\n",
					src_info->identifier);
			goto bail;
		}

		qt_src_ctx->src = sg_manager_source_find(&qt_ctx->mgr,
				device_id, input_id);

		if ( !qt_src_ctx->src )
		{
			log_warn("failed to acquire source %s (%s)\n",
					src_info->description,
					src_info->identifier);
			goto bail;
		}
	}
	else
	{
		qt_src_ctx->src = sg_manager_source_default_find(&qt_ctx->mgr);

		if ( !qt_src_ctx->src )
		{
			log_warn("failed to acquire default source\n");
			goto bail;
		}
	}

	if ( sg_source_acquire(qt_src_ctx->src) )
		goto bail;

	src_info_from_src(&src_ctx->src_info, qt_src_ctx->src);

	if ( mutex_unlock(&qt_ctx->mutex) )
		return -1;

	qt_src_ctx->capturing = 0;

	src_ctx->release = source_release;
	src_ctx->format_validate = source_format_validate;
	src_ctx->format_bind = source_format_bind;
	src_ctx->start_capture = source_capture_start;
	src_ctx->stop_capture = source_capture_stop;
	src_ctx->priv = qt_src_ctx;

	return 0;

bail:
	mutex_unlock(&qt_ctx->mutex);
	free(qt_src_ctx);
	return -1;
}

static unsigned int
notify_thread_proc(void * data)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)data;
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;

	struct timespec wait_ts = { 1, 0 };

	int last_source_count = sg_manager_source_count(&qt_ctx->mgr);

	while ( qt_ctx->notifying )
	{
		int source_count;

		if ( mutex_lock(&qt_ctx->mutex) )
			return -1;

		if ( sg_manager_update(&qt_ctx->mgr) )
		{
			mutex_unlock(&qt_ctx->mutex);
			return -1;
		}

		source_count = sg_manager_source_count(&qt_ctx->mgr);

		if ( mutex_unlock(&qt_ctx->mutex) )
			return -1;

		if ( source_count != last_source_count &&
				sapi_ctx->notify_callback )
		{
			last_source_count = source_count;
			sapi_ctx->notify_callback(sapi_ctx,
					sapi_ctx->notify_data);
		}

		if ( nanosleep(&wait_ts, 0) )
		{
			if ( errno != -EINTR )
			{
				log_error("failed nanosleep() %d\n", errno);
				return -1;
			}
		}
	}

	return 0;
}

static int
notify_thread_stop(struct sapi_context * sapi_ctx)
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;
	int ret;

	qt_ctx->notifying = 0;

	if ( (ret = vc_thread_join(&qt_ctx->notify_thread)) )
	{
		log_error("failed vc_thread_join() notify thread %d\n", ret);
		return -1;
	}

	return 0;
}

static int
monitor_sources(struct sapi_context * sapi_ctx)
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;
	int ret;

	if ( !sapi_ctx->notify_callback && qt_ctx->notifying )
		return notify_thread_stop(sapi_ctx);

	qt_ctx->notifying = 1;

	if ( (ret = vc_create_thread(&qt_ctx->notify_thread, notify_thread_proc,
			sapi_ctx, &qt_ctx->notify_thread_id)) )
	{
		log_error("failed vc_thread_create() for notify thread %d\n",
				ret);
		return -1;
	}

	return 0;
}

static int
sapi_qt_release(struct sapi_context * sapi_ctx)
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;

	if ( qt_ctx->notifying )
		notify_thread_stop(sapi_ctx);

	return sapi_release(sapi_ctx);
}

static void
sapi_qt_destroy(struct sapi_context * sapi_ctx )
{
	struct sapi_qt_context * qt_ctx =
		(struct sapi_qt_context *)sapi_ctx->priv;

	sg_manager_destroy(&qt_ctx->mgr);

	ExitMovies();

	vc_mutex_destroy(&qt_ctx->mutex);

	free(sapi_ctx->user_src_list.list);
	free(qt_ctx);
}

int
sapi_qt_initialize(struct sapi_context * sapi_ctx)
{
	struct sapi_qt_context * qt_ctx;

	qt_ctx = calloc(1, sizeof(*qt_ctx));

	if ( !qt_ctx )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	if ( vc_mutex_init(&qt_ctx->mutex) )
	{
		log_error("failed vc_mutex_init()\n");
		return -1;
	}

	EnterMovies();

	sapi_ctx->identifier = identifier;
	sapi_ctx->description = description;
	sapi_ctx->priv = qt_ctx;

	sapi_ctx->acquire = sapi_acquire;
	sapi_ctx->release = sapi_qt_release;
	sapi_ctx->destroy = sapi_qt_destroy;
	sapi_ctx->scan_sources = scan_sources;
	sapi_ctx->monitor_sources = monitor_sources;
	sapi_ctx->acquire_source = source_acquire;

	if ( sg_manager_init(&qt_ctx->mgr) )
		goto bail;

	return 0;

bail:
	free(qt_ctx);
	return -1;
}

