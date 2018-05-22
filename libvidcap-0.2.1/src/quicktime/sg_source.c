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

#include "logging.h"
#include "sg_source.h"

static const char uvc_device_name[] = "USB Video Class Video";

int sg_source_init(struct sg_source * src, int device_id, int input_id,
		Str63 device_name, Str63 input_name)
{
	src->device_id = device_id;
	src->input_id = input_id;
	memcpy(src->device_name, device_name, sizeof(Str63));
	memcpy(src->input_name, input_name, sizeof(Str63));

	src->is_uvc = !memcmp(uvc_device_name, &src->device_name[1],
			sizeof(uvc_device_name));

	return 0;
}

int sg_source_anon_init(struct sg_source * src)
{
	memset(src, 0, sizeof(*src));
	src->device_id = -1;
	src->input_id = -1;
	return 0;
}

int sg_source_destroy(struct sg_source * mgr)
{
	return 0;
}

int sg_source_match(struct sg_source * src, int device_id, int input_id)
{
	return device_id == src->device_id && input_id == src->input_id;
}

int sg_source_acquire(struct sg_source * src)
{
	OSErr err;

	src->channel = 0;

	if ( !(src->grabber = OpenDefaultComponent(SeqGrabComponentType, 0)) )
	{
		log_error("failed OpenDefaultComponent()\n");
		return -1;
	}

	src->acquired = 1;

	if ( (err = SGInitialize(src->grabber)) )
	{
		log_error("failed SGInitialize() %d\n", err);
		goto bail;
	}

	if ( (err = SGSetDataRef(src->grabber, 0, 0, seqGrabDontMakeMovie)) )
	{
		log_error("failed SGSetDataRef() %d\n", err);
		goto bail;
	}

	if ( (err = SGNewChannel(src->grabber, VideoMediaType, &src->channel)) )
	{
		if ( err == couldntGetRequiredComponent )
		{
			sg_source_release(src);
			return 1;
		}

		log_error("failed SGNewChannel() %d\n", err);
		goto bail;
	}

	if ( src->input_id >= 0 && src->device_id >= 0 )
	{
		VideoDigitizerComponent digi;
		VDCompressionListHandle compression_list =
			(VDCompressionListHandle)NewHandle(0);
		int compression_list_len;

		if ( (err = SGSetChannelDevice(src->channel,
						src->device_name)) )
		{
			log_error("failed SGSetChannelDevice() %d\n", err);
			goto bail;
		}

		if ( (err = SGSetChannelDeviceInput(src->channel,
						src->input_id)) )
		{
			log_error("failed SGSetChannelDeviceInput() %d\n", err);
			goto bail;
		}

		digi = SGGetVideoDigitizerComponent(src->channel);

		if ( (err = VDGetDigitizerInfo(digi, &src->info)) )
		{
			log_error("failed VDGetDigitizerInfo() %d\n", err);
			goto bail;
		}

		if ( (err = VDGetDataRate(digi, &src->ms_per_frame,
						&src->max_fps,
						&src->bytes_per_second)) )
		{
			log_error("failed VDGetDataRate() %d\n", err);
			goto bail;
		}

		if ( (err = VDGetCompressionTypes(digi, compression_list)) )
		{
			log_error("failed VDGetCompressionTypes() %d\n", err);
			goto bail;
		}

		compression_list_len = sizeof(**compression_list) /
			sizeof(VDCompressionList);

		if ( compression_list_len > 1 )
			log_warn("compression list of length %d\n",
					compression_list_len);

		src->native_pixel_format = (*compression_list)->cType;

		log_debug("native pixel format %c%c%c%c\n",
				(char)(src->native_pixel_format >> 24),
				(char)(src->native_pixel_format >> 16),
				(char)(src->native_pixel_format >> 8),
				(char)(src->native_pixel_format >> 0));

		DisposeHandle((Handle)compression_list);
	}

	return 0;

bail:
	sg_source_release(src);
	return -1;
}

int sg_source_release(struct sg_source * src)
{
	OSErr err;

	src->acquired = 0;

	if ( !src->grabber )
		return 0;

	if ( src->channel )
	{
		if ( (err = SGDisposeChannel(src->grabber, src->channel)) )
		{
			log_error("failed SGDisposeChannel() %d\n", err);
			return -1;
		}
	}

	gworld_destroy(&src->gi);

	if ( (err = CloseComponent(src->grabber)) )
	{
		log_error("failed CloseComponent() %d\n", err);
		return -1;
	}

	return 0;
}

int sg_source_format_validate(struct sg_source * src, int width, int height,
		float fps)
{
	if ( width > src->info.maxDestWidth || height > src->info.maxDestHeight )
		return 0;

	if ( width < src->info.minDestWidth || height < src->info.minDestHeight )
		return 0;

	if ( fps > FixedToFloat(src->max_fps) )
		return 0;

	return 1;
}

int sg_source_format_set(struct sg_source *src , int width, int height,
		float fps, OSType pixel_format)
{
	OSErr err;
	Rect dimensions = { .left = 0, .top = 0,
		.right = width, .bottom = height };

	gworld_destroy(&src->gi);

	if ( gworld_init(&src->gi, &dimensions, pixel_format) )
		return -1;

	if ( (err = SGSetGWorld(src->channel, gworld_get(&src->gi), 0)) )
	{
		log_error("failed SGSetGWorld() %d\n", err);
		return -1;
	}

	if ( (err = SGSetChannelBounds(src->channel, &dimensions)) )
	{
		log_error("failed SGSetChannelBounds() %d\n", err);
		return -1;
	}

	if ( (err = SGSetFrameRate(src->channel, FloatToFixed(fps))) )
	{
		log_error("failed SGSetFrameRate() %d\n", err);
		return -1;
	}

	return 0;
}

int sg_source_image_desc_get(struct sg_source * src, ImageDescriptionHandle desc)
{
	OSErr err;

	if ( (err = SGGetChannelSampleDescription(src->channel, (Handle)desc)) )
	{
		log_error("failed SGGetChannelSampleDescription() %d\n", err);
		return -1;
	}

	return 0;
}

int sg_source_capture_start(struct sg_source * src, SGDataUPP capture_callback,
		long callback_data)
{
	OSErr err;

	if ( (err = SGSetChannelUsage(src->channel, seqGrabRecord)) )
	{
		log_error("failed SGSetChannelUsage() %d\n", err);
		return -1;
	}

	if ( (err = SGSetDataProc(src->grabber, capture_callback,
					callback_data)) )
	{
		log_error("failed SGSetDataProc() %d\n", err);
		return -1;
	}

	if ( (err = SGStartRecord(src->grabber)) )
	{
		log_error("failed SGStartRecord() %d\n", err);
		return -1;
	}

	if ( (err = SGGetChannelTimeScale(src->channel, &src->time_scale)) )
	{
		log_error("failed SGGetChannelTimeScale() %d\n", err);
		return -1;
	}

	return 0;
}

int sg_source_capture_stop(struct sg_source * src)
{
	OSErr err;

	if ( (err = SGStop(src->grabber)) )
	{
		log_error("failed SGStop() %d\n", err);
		return -1;
	}

	return 0;
}

int sg_source_capture_poll(struct sg_source * src)
{
	OSErr err;

	if ( (err = SGIdle(src->grabber)) )
	{
		if ( err == -2211 )
		{
			log_warn("during SGIdle() device removed\n");
			return 1;
		}

		log_error("failed SGIdle() %d\n", err);
		return -1;
	}

	return 0;
}
