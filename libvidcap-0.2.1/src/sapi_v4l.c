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

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "conv.h"
#include "hotlist.h"
#include "logging.h"
#include "sapi.h"

static const char * identifier = "video4linux";
static const char * description = "Video for Linux (v4l) video capture API";

enum
{
	device_path_max = 128,
	device_num_max = 16,
	v4l_fps_mask = 0x003f0000,
	v4l_fps_shift = 16,
};

struct sapi_v4l_context
{
	vc_mutex mutex;
	vc_thread notify_thread;
	unsigned int notify_thread_id;
	int notifying;
	struct sapi_src_list acquired_src_list;
};

struct sapi_v4l_src_context
{
	struct sapi_v4l_context * v4l_ctx;

	int fd;
	char device_path[device_path_max];
	int channel;

	struct video_capability caps;
	struct video_picture picture;
	struct video_window window;
	struct video_mbuf mbuf;

	int capturing;
	vc_thread capture_thread;
	unsigned int capture_thread_id;
};

static int
parse_src_identifier(const char * identifier,
		char * device_path,
		int device_path_len,
		int * channel)
{
	char * separator;
	char * ident_end;
	int ident_len;
	int i;

	ident_end = memchr(identifier, 0, device_path_len);

	if ( !ident_end )
	{
		log_error("identifier too long\n");
		return -1;
	}

	ident_len = ident_end - identifier;

	separator = memchr(identifier, ',', ident_len);

	if ( !separator )
	{
		log_error("separator ',' not found in source identifier\n");
		return -1;
	}

	memcpy(device_path, identifier, separator - identifier);
	device_path[separator - identifier] = 0;

	i = sscanf(separator + 1, "%d", channel);

	if ( i != 1 )
	{
		log_error("failed parsing source channel number\n");
		return -1;
	}

	return 0;
}

static int
map_fourcc_to_palette(int fourcc, __u16 * palette)
{
	switch ( fourcc )
	{
	case VIDCAP_FOURCC_I420:
		*palette = VIDEO_PALETTE_YUV420P;
		break;

	case VIDCAP_FOURCC_RGB24:
		*palette = VIDEO_PALETTE_RGB24;
		break;

	case VIDCAP_FOURCC_YUY2:
		*palette = VIDEO_PALETTE_YUYV;
		break;

	case VIDCAP_FOURCC_2VUY:
		*palette = VIDEO_PALETTE_UYVY;
		break;

	case VIDCAP_FOURCC_RGB32:
		*palette = VIDEO_PALETTE_RGB32;
		break;

	case VIDCAP_FOURCC_RGB555:
		*palette = VIDEO_PALETTE_RGB555;
		break;

	case VIDCAP_FOURCC_YVU9:
		return 1;

	default:
		return -1;
	}

	return 0;
}

static int
map_palette_to_fourcc(__u16 palette, int * fourcc)
{
	switch ( palette )
	{
	case VIDEO_PALETTE_YUV420P:
		*fourcc = VIDCAP_FOURCC_I420;
		break;

	case VIDEO_PALETTE_RGB24:
		*fourcc = VIDCAP_FOURCC_RGB24;
		break;

	case VIDEO_PALETTE_YUYV:
		*fourcc = VIDCAP_FOURCC_YUY2;
		break;

	case VIDEO_PALETTE_UYVY:
		*fourcc = VIDCAP_FOURCC_2VUY;
		break;

	case VIDEO_PALETTE_RGB32:
		*fourcc = VIDCAP_FOURCC_RGB32;
		break;

	case VIDEO_PALETTE_RGB555:
		*fourcc = VIDCAP_FOURCC_RGB555;
		break;

	default:
		return -1;
	}

	return 0;
}

#if 0
static void
src_list_debug(const struct sapi_src_list * src_list, const char * name)
{
	int i;
	for ( i = 0; i < src_list->len; ++i )
		log_debug("%10s: %2d: %s\n", name, i,
				src_list->list[i].identifier);
}
#endif

/* Searches src_list starting at index for a vidcap_src_info that
 * matches device_path. The source identifier contains the device
 * path and the channel. For sapi_v4l's purposes, we do _not_ match
 * the channel -- just the device path.
 */
static const struct vidcap_src_info *
src_list_device_match_next(const struct sapi_src_list * src_list,
		const char * device_path, int * index)
{
	int i;

	for ( i = *index; i < src_list->len; ++i )
	{
		const struct vidcap_src_info * src_info = &src_list->list[i];
		char src_device_path[device_path_max];
		int src_device_channel;

		memset(src_device_path, 0, sizeof(src_device_path));

		if ( parse_src_identifier(src_info->identifier,
					src_device_path,
					sizeof(src_device_path),
					&src_device_channel) )
		{
			log_warn("src_list_device_match_next: invalid src "
					"identifier \"%s\"\n",
					src_info->identifier);
			return 0;
		}

		if ( strncmp(src_device_path, device_path,
					sizeof(src_device_path)) == 0 )
		{
			*index = i + 1;
			return src_info;
		}
	}

	*index = i;

	return 0;
}

static const struct vidcap_src_info *
src_list_src_match_next(const struct sapi_src_list * src_list,
		const struct vidcap_src_info * src_info, int * index)
{
	char device_path[device_path_max];
	int device_channel;

	if ( parse_src_identifier(src_info->identifier, device_path,
				sizeof(device_path), &device_channel) )
	{
		log_warn("src_list_src_match_next: invalid src "
				"identifier \"%s\"\n",
				src_info->identifier);
		return 0;
	}

	return src_list_device_match_next(src_list, device_path, index);
}

/* The idea here is to find a vidcap_src_info in the src_list that
 * matches the src_info parameter. We then remove the matched src_info
 * with memmove().
 *
 *                0 1 2 3 4 5
 * original list: p q r s t u  len=6
 * remove 's':    p q r t u u  len=5
 */
static int
src_list_src_remove(struct sapi_src_list * src_list,
		const struct vidcap_src_info * src_info)
{
	int match_index = 0;
	const struct vidcap_src_info * matched_src_info =
		src_list_src_match_next(src_list, src_info, &match_index);

	if ( !matched_src_info )
		return -1;

	memmove(&src_list->list[match_index - 1],
			&src_list->list[match_index],
			sizeof(*matched_src_info) *
			(src_list->len - match_index));

	src_list->len--;

	return 0;
}

static int
src_list_src_append(struct sapi_src_list * src_list,
		const struct vidcap_src_info * src_info)
{
	src_list->len++;
	src_list->list = realloc(src_list->list,
			src_list->len * sizeof(*src_info));

	if ( !src_list->list )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	memcpy(&src_list->list[src_list->len - 1], src_info, sizeof(*src_info));

	return 0;
}

static int
src_list_device_append(struct sapi_src_list * src_list,
		const char * device_path, int channel,
		const char * device_name, const char * channel_name)
{
	struct vidcap_src_info src_info;

	memset(&src_info, 0, sizeof(src_info));

	snprintf(src_info.identifier, sizeof(src_info.identifier),
			"%s,%d", device_path, channel);

	snprintf(src_info.description, sizeof(src_info.description),
			"%s %s", device_name, channel_name);

	return src_list_src_append(src_list, &src_info);
}

static int
src_list_device_scan(struct sapi_src_list * src_list,
		const char * device_path)
{
	int ret = 0;
	int fd;
	int i;
	struct video_capability caps;

	fd = open(device_path, O_RDONLY);

	if ( fd == -1 )
		return 1;

	/* Ensures file descriptor is closed on exec() */
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	memset(&caps, 0, sizeof(caps));

	if ( ioctl(fd, VIDIOCGCAP, &caps) == -1 )
	{
		log_warn("failed to get capabilities for %s\n", device_path);
		ret = -1;
		goto bail;
	}

	if ( caps.type != VID_TYPE_CAPTURE )
	{
		ret = -1;
		goto bail;
	}

	for ( i = 0; i < caps.channels; ++i )
	{
		struct video_channel channel;

		memset(&channel, 0, sizeof(channel));

		channel.channel = i;

		if ( ioctl(fd, VIDIOCGCHAN, &channel) == -1 )
		{
			log_warn("failed VIDIOCGCHAN for %s\n", device_path);
			continue;
		}

		if ( src_list_device_append(src_list, device_path,
					channel.channel, caps.name,
					channel.name) )
		{
			ret = -1;
			goto bail;
		}
	}

bail:

	if ( close(fd) == -1 )
		log_warn("failed to close fd %d for %s (%s)\n",
				fd, device_path, strerror(errno));

	return ret;
}

/* Generate device path strings of the sequence:
 *   /dev/video
 *   /dev/video0
 *   /dev/video/0
 *   /dev/video1
 *   /dev/video/1
 *   ...
 *   /dev/video{device_num_max - 1}
 *   /dev/video/{device_num_max - 1}
 */
static int
device_path_generate(char * device_path, int device_path_len, int * state)
{
	static const char device_prefix[] = "/dev/video";
	const int device_num = (*state - 1) / 2;

	memset(device_path, 0, device_path_len);

	if ( *state == 0 )
		snprintf(device_path, device_path_len, "%s",
				device_prefix);
	else if ( device_num >= device_num_max )
		return 0;
	else if ( *state % 2 )
		snprintf(device_path, device_path_len, "%s%d",
				device_prefix, device_num);
	else
		snprintf(device_path, device_path_len, "%s/%d",
				device_prefix, device_num);

	*state += 1;

	return 1;
}

/* The scanning semantics are a bit tricky. The src_list pointer parameter
 * may or may not have anything in it. If we were so inclined, we could
 * potentially reuse the memory in src_list->list; however we just free()
 * the memory and start over.
 *
 * One of the tricky bits is that video devices are open()ed exclusively.
 * This means that we cannot simply scan with open() and ioctl() calls to
 * get the complete device list -- we must also include devices that have
 * already been acquired.
 *
 * To this end, we keep an acquired device list as part of the
 * sapi_v4l_context structure. This list must be taken into account and
 * managed when we scan, acquire, and release sources.
 *
 * Complicating matters further is the fact that a single v4l device may
 * have multiple "channels". We list each of these channels as a separate
 * vidcap source, but we can only capture from a single v4l channel from
 * any particular source. We enforce this exclusivity by matching sources
 * using the device paths during aquired source list management.
 *
 * For example, consider this case:
 *
 *   src0 --> /dev/video0 channel 0
 *   src1 --> /dev/video0 channel 1
 *   src2 --> /dev/video1 channel 0
 *
 * If a user acquires src0, this implies that src1 is unavailable, so we
 * put both src0 and src1 into the acquired list! Since src2 has a
 * different device path, it remains available for acquisition.
 */
static int
scan_sources(struct sapi_context * sapi_ctx, struct sapi_src_list * src_list)
{
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;
	char device_path[device_path_max];
	int generate_index = 0;

#if 0
	log_debug("scan_sources start\n");
	src_list_debug(&sapi_ctx->user_src_list, "usr");
	src_list_debug(&v4l_ctx->acquired_src_list, "acq");
#endif

	if ( sapi_ctx->user_src_list.list )
	{
		free(sapi_ctx->user_src_list.list);
		sapi_ctx->user_src_list.list = 0;
		sapi_ctx->user_src_list.len = 0;
	}

	while ( device_path_generate(device_path, sizeof(device_path),
				&generate_index) )
	{
		const struct vidcap_src_info * acquired_src_info = 0;
		int match_index = 0;

		while ( (acquired_src_info = src_list_device_match_next(
						&v4l_ctx->acquired_src_list,
						device_path, &match_index)) )
		{
			if ( src_list_src_append(src_list,
						acquired_src_info) )
				return -1;
		}

		if ( !acquired_src_info )
			if ( src_list_device_scan(src_list, device_path) < 0 )
				return -1;
	}

#if 0
	log_debug("scan_sources finish\n");
	src_list_debug(&sapi_ctx->user_src_list, "usr");
	src_list_debug(&v4l_ctx->acquired_src_list, "acq");
#endif

	return src_list->len;
}

static int
capture_kickoff(struct sapi_src_context * src_ctx,
		unsigned int frame_count)
{
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;

	struct video_mmap vmap;

	vmap.frame  = frame_count % v4l_src_ctx->mbuf.frames;
	vmap.height = v4l_src_ctx->window.height;
	vmap.width  = v4l_src_ctx->window.width;
	vmap.format = v4l_src_ctx->picture.palette;

	if ( ioctl(v4l_src_ctx->fd, VIDIOCMCAPTURE, &vmap) == -1 )
	{
		log_error("failed VIDIOCMCAPTURE %d\n", errno);
		return -1;
	}

	return 0;
}

static unsigned int
capture_thread_proc(void * data)
{
	struct sapi_src_context * src_ctx =
		(struct sapi_src_context *)data;
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;
	const int capture_size = conv_fmt_size_get(
			src_ctx->fmt_native.width,
			src_ctx->fmt_native.height,
			src_ctx->fmt_native.fourcc);

	const char * fb_base = mmap(0, v4l_src_ctx->mbuf.size,
			PROT_READ, MAP_SHARED, v4l_src_ctx->fd, 0);

	unsigned int started_frame_count = 0;
	unsigned int capture_frame_count = 0;

	if ( fb_base == (void *)-1 )
	{
		log_error("failed mmap() %d\n", errno);
		return -1;
	}

	if ( capture_kickoff(src_ctx, started_frame_count) )
		goto bail;

	while ( v4l_src_ctx->capturing )
	{
		const unsigned int capture_frame =
			capture_frame_count % v4l_src_ctx->mbuf.frames;

		if ( capture_kickoff(src_ctx, ++started_frame_count) )
			goto bail;

		if ( ioctl( v4l_src_ctx->fd, VIDIOCSYNC,
					&capture_frame ) == -1 )
		{
			log_error("failed VIDIOCSYNC %d\n", errno);
			goto bail;
		}

		sapi_src_capture_notify(src_ctx, (char *)(fb_base +
				v4l_src_ctx->mbuf.offsets[capture_frame]),
				capture_size,
				0,
				0);

		++capture_frame_count;
	}

	/* Sync with the last frame when capture has stopped (since we
	 * have overlapped capture kickoffs).
	 */
	{
		const unsigned int capture_frame =
			capture_frame_count % v4l_src_ctx->mbuf.frames;

		if ( ioctl( v4l_src_ctx->fd, VIDIOCSYNC,
					&capture_frame ) == -1 )
		{
			log_error("failed VIDIOCSYNC (2) %d\n", errno);
			goto bail;
		}
	}


	if ( munmap((void *)fb_base, v4l_src_ctx->mbuf.size) == -1 )
	{
		log_error("failed munmap() %d\n", errno);
		return -1;
	}

	return 0;

bail:
	if ( munmap((void *)fb_base, v4l_src_ctx->mbuf.size) == -1 )
		log_warn("failed munmap() %d\n", errno);

	return -1;
}

static int
source_capture_start(struct sapi_src_context * src_ctx)
{
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;
	int ret;

	v4l_src_ctx->capturing = 1;

	if ( (ret = vc_create_thread(&v4l_src_ctx->capture_thread,
				capture_thread_proc, src_ctx,
				&v4l_src_ctx->capture_thread_id)) )
	{
		log_error("failed vc_create_thread() for capture thread %d\n",
				ret);
		return -1;
	}

	return 0;
}

static int
source_capture_stop(struct sapi_src_context * src_ctx)
{
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;
	int ret;

	v4l_src_ctx->capturing = 0;

	if ( (ret = vc_thread_join(&v4l_src_ctx->capture_thread)) )
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
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;

	*fmt_native = *fmt_nominal;

	__u16 palette;

	if ( fmt_nominal->width < v4l_src_ctx->caps.minwidth ||
			fmt_nominal->width > v4l_src_ctx->caps.maxwidth ||
			fmt_nominal->height < v4l_src_ctx->caps.minheight ||
			fmt_nominal->height > v4l_src_ctx->caps.maxheight )
		return 0;

	if ( map_fourcc_to_palette(fmt_nominal->fourcc, &palette) )
		return 0;

	if ( map_palette_to_fourcc(v4l_src_ctx->picture.palette,
				&fmt_native->fourcc) )
		return 0;

	if ( !sapi_can_convert_native_to_nominal(fmt_native, fmt_nominal) )
		return 0;

	return 1;
}

static int
source_format_bind(struct sapi_src_context * src_ctx,
		const struct vidcap_fmt_info * fmt_info)
{
	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;

	memcpy(&src_ctx->fmt_native, fmt_info, sizeof(*fmt_info));

	if ( map_palette_to_fourcc(v4l_src_ctx->picture.palette,
				&src_ctx->fmt_native.fourcc) )
	{
		log_warn("failed map_palette_to_fourcc %d\n",
				v4l_src_ctx->picture.palette);
		return -1;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCSPICT, &v4l_src_ctx->picture) == -1 )
	{
		log_warn("failed to set v4l picture parameters\n");
		return -1;
	}

	/* Only set the parameters we know/care about. */
	v4l_src_ctx->window.x = 0;
	v4l_src_ctx->window.y = 0;
	v4l_src_ctx->window.width = fmt_info->width;
	v4l_src_ctx->window.height = fmt_info->height;
	/* TODO: for pwc, framerate is encoded in flags. For other
	 * cameras, who knows.
	 */
	v4l_src_ctx->window.flags =
		(v4l_src_ctx->window.flags & ~v4l_fps_mask) |
		(((src_ctx->fmt_native.fps_numerator /
		   src_ctx->fmt_native.fps_denominator) << v4l_fps_shift) &
		 v4l_fps_mask);

	if ( ioctl(v4l_src_ctx->fd, VIDIOCSWIN, &v4l_src_ctx->window) == -1 )
	{
		log_warn("failed to set v4l window parameters x=%d y=%d "
				"w=%d h=%d fl=0x%08x\n",
				v4l_src_ctx->window.x,
				v4l_src_ctx->window.y,
				v4l_src_ctx->window.width,
				v4l_src_ctx->window.height,
				v4l_src_ctx->window.flags);
		return -1;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCGWIN, &v4l_src_ctx->window) == -1 )
	{
		log_warn("failed to get v4l window parameters\n");
		return -1;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCGMBUF, &v4l_src_ctx->mbuf) == -1 )
	{
		log_warn("failed to get v4l memory info for %s\n",
				v4l_src_ctx->device_path);
		return -1;
	}

	return 0;
}

static int
source_release(struct sapi_src_context * src_ctx)
{
	int ret = 0;

	struct sapi_v4l_src_context * v4l_src_ctx =
		(struct sapi_v4l_src_context *)src_ctx->priv;

	/* Remove all matching sources from the acquired src list */
	while ( !src_list_src_remove(&v4l_src_ctx->v4l_ctx->acquired_src_list,
				&src_ctx->src_info) )
		;

	if ( v4l_src_ctx->fd < 0 )
	{
		log_error("invalid fd (%d) for %s\n", v4l_src_ctx->fd,
				v4l_src_ctx->device_path);
		ret = -1;
	}
	else
	{
		if ( close(v4l_src_ctx->fd) == -1 )
		{
			log_error("failed to close fd %d for %s\n",
					v4l_src_ctx->fd,
					v4l_src_ctx->device_path);
			ret = -1;
		}
	}

	free(v4l_src_ctx);

	return ret;
}

static int
source_acquire(struct sapi_context * sapi_ctx,
		struct sapi_src_context * src_ctx,
		const struct vidcap_src_info * src_info)
{
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;
	struct sapi_v4l_src_context * v4l_src_ctx = 0;
	const struct vidcap_src_info * acquired_src_info;
	int match_index;

	if ( !src_info )
	{
		int i;

		if ( scan_sources(sapi_ctx, &sapi_ctx->user_src_list) <= 0 )
			return -1;

		/* Find the first src_info from user_src_list that is
		 * not in the acquired list.
		 */
		for ( i = 0; i < sapi_ctx->user_src_list.len; ++i )
		{
			src_info = &sapi_ctx->user_src_list.list[i];
			match_index = 0;

			if ( !src_list_src_match_next(
						&v4l_ctx->acquired_src_list,
						src_info, &match_index) )
			{
				break;
			}
			else
			{
				src_info = 0;
			}
		}

		if ( !src_info )
		{
			log_error("failed finding default source to acquire\n");
			return -1;
		}
	}
	else
	{
		match_index = 0;

		if ( src_list_src_match_next(&v4l_ctx->acquired_src_list,
					src_info, &match_index) )
		{
			log_error("source \"%s\" already acquired\n",
					src_info->identifier);
			return -1;
		}
	}

	memcpy(&src_ctx->src_info, src_info, sizeof(src_ctx->src_info));

	v4l_src_ctx = calloc(1, sizeof(*v4l_src_ctx));

	if ( !v4l_src_ctx )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	v4l_src_ctx->v4l_ctx = v4l_ctx;

	if ( parse_src_identifier(src_info->identifier,
				v4l_src_ctx->device_path,
				sizeof(v4l_src_ctx->device_path),
				&v4l_src_ctx->channel) )
	{
		log_error("invalid source identifier (%s)\n",
				src_info->identifier);
		goto bail;
	}

	v4l_src_ctx->fd = open(v4l_src_ctx->device_path, O_RDONLY);

	if ( v4l_src_ctx->fd == -1 )
	{
		log_error("failed to open %s (%s)\n",
				v4l_src_ctx->device_path,
				strerror(errno));
		goto bail;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCGCAP, &v4l_src_ctx->caps) == -1 )
	{
		log_warn("failed to get v4l capability parameters for %s\n",
				v4l_src_ctx->device_path);
		goto bail;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCGWIN, &v4l_src_ctx->window) == -1 )
	{
		log_warn("failed to get v4l window parameters for %s\n",
				v4l_src_ctx->device_path);
		goto bail;
	}

	if ( ioctl(v4l_src_ctx->fd, VIDIOCGPICT, &v4l_src_ctx->picture) == -1 )
	{
		log_warn("failed to get v4l picture parameters for %s\n",
				v4l_src_ctx->device_path);
		goto bail;
	}

	/* Add all sources with the same device path to the acquired
	 * source list.
	 */
	match_index = 0;
	while ( (acquired_src_info = src_list_src_match_next(
					&sapi_ctx->user_src_list,
					src_info, &match_index)) )
	{
		if ( src_list_src_append(&v4l_ctx->acquired_src_list,
					acquired_src_info) )
			return -1;
	}

	v4l_src_ctx->capturing = 0;

	src_ctx->release = source_release;
	src_ctx->format_validate = source_format_validate;
	src_ctx->format_bind = source_format_bind;
	src_ctx->start_capture = source_capture_start;
	src_ctx->stop_capture = source_capture_stop;
	src_ctx->priv = v4l_src_ctx;

	return 0;

bail:
	free(v4l_src_ctx);
	return -1;
}

static unsigned int
notify_thread_proc(void * data)
{
	struct sapi_context * sapi_ctx = (struct sapi_context *)data;
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;

	struct timespec wait_ts = { 1, 0 };

	while ( v4l_ctx->notifying )
	{
		vc_mutex_lock(&v4l_ctx->mutex);

		/* TODO: look for devices */

		vc_mutex_unlock(&v4l_ctx->mutex);

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
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;
	int ret;

	v4l_ctx->notifying = 0;

	if ( (ret = vc_thread_join(&v4l_ctx->notify_thread)) )
	{
		log_error("failed vc_thread_join() notify thread %d\n", ret);
		return -1;
	}

	return 0;
}

static int
monitor_sources(struct sapi_context * sapi_ctx)
{
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;
	int ret;

	if ( !sapi_ctx->notify_callback && v4l_ctx->notifying )
		return notify_thread_stop(sapi_ctx);

	v4l_ctx->notifying = 1;

	if ( (ret = vc_create_thread(&v4l_ctx->notify_thread,
					notify_thread_proc, sapi_ctx,
					&v4l_ctx->notify_thread_id)) )
	{
		log_error("failed vc_create_thread() for notify thread %d\n",
				ret);
		return -1;
	}

	return 0;
}

static void
sapi_v4l_destroy(struct sapi_context * sapi_ctx)
{
	struct sapi_v4l_context * v4l_ctx =
		(struct sapi_v4l_context *)sapi_ctx->priv;

	vc_mutex_destroy(&v4l_ctx->mutex);

	free(sapi_ctx->user_src_list.list);
	free(v4l_ctx);
}

int
sapi_v4l_initialize(struct sapi_context * sapi_ctx)
{
	struct sapi_v4l_context * v4l_ctx;

	v4l_ctx = calloc(1, sizeof(*v4l_ctx));

	if ( !v4l_ctx )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	if ( vc_mutex_init(&v4l_ctx->mutex) )
	{
		log_error("failed vc_mutex_init()\n");
		return -1;
	}

	sapi_ctx->identifier = identifier;
	sapi_ctx->description = description;
	sapi_ctx->priv = v4l_ctx;

	/* TODO: Use pwc-specific ioctls. VIDIOCPWCSCQUAL */

	sapi_ctx->acquire = sapi_acquire;
	sapi_ctx->release = sapi_release;
	sapi_ctx->destroy = sapi_v4l_destroy;
	sapi_ctx->scan_sources = scan_sources;
	sapi_ctx->monitor_sources = monitor_sources;
	sapi_ctx->acquire_source = source_acquire;

	return 0;
}

