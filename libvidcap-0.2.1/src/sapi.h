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

#ifndef _SAPI_H
#define _SAPI_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define VIDCAP_INVALID_USER_DATA	((void *)-1)

#include "sapi_context.h"

#ifdef __cplusplus
extern "C" {
#endif

int
sapi_acquire(struct sapi_context *);

int
sapi_release(struct sapi_context *);

int
sapi_src_capture_notify(struct sapi_src_context * src_ctx,
		char * video_data, int video_data_size,
		int stride,
		int error_status);

int
sapi_src_format_list_build(struct sapi_src_context * src_ctx);

int
sapi_can_convert_native_to_nominal(const struct vidcap_fmt_info * fmt_native,
		const struct vidcap_fmt_info * fmt_nominal);

#ifdef __cplusplus
}
#endif

#endif
