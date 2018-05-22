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
#include "sg_manager.h"

int sg_manager_init(struct sg_manager * mgr)
{
	mgr->source_list_len = 0;
	mgr->source_list = 0;
	return 0;
}

int sg_manager_destroy(struct sg_manager * mgr)
{
	if ( mgr->source_list )
		free(mgr->source_list);

	return 0;
}

int sg_manager_update(struct sg_manager * mgr)
{
	SGDeviceList device_list;
	SGDeviceListPtr dlr;
	OSErr err;
	struct sg_source src;
	int ret = 0;
	int i;
	int input_index = 0;

	/* Short circuit if the list has already been composed. Getting a
	 * new device list proves to be futile.
	 */
	if ( mgr->source_list )
		return 0;

	if ( sg_source_anon_init(&src) )
		return -1;

	if ( (ret = sg_source_acquire(&src)) )
	{
		if ( ret < 0 )
			return -1;

		/* There are no sources */
		sg_source_destroy(&src);
		return 0;
	}

	if ( (err = SGGetChannelDeviceList(src.channel,
					sgDeviceListIncludeInputs,
					&device_list)) )
	{
		log_error("failed SGGetChannelDeviceList() %d\n", err);
		ret = -1;
		goto bail_source;
	}

	dlr = *device_list;

	for ( i = 0; i < dlr->count; ++i )
	{
		if ( !dlr->entry[i].inputs )
			continue;

		mgr->source_list_len += (*(dlr->entry[i].inputs))->count;
	}

	mgr->source_list = calloc(mgr->source_list_len,
			sizeof(struct sg_source));

	if ( !mgr->source_list )
	{
		log_oom(__FILE__, __LINE__);
		return -1;
	}

	for ( i = 0; i < dlr->count; ++i )
	{
		SGDeviceInputListPtr ilr;
		int j;

		if ( !dlr->entry[i].inputs )
			continue;

		ilr = *(dlr->entry[i].inputs);

		for ( j = 0; j < ilr->count; ++j )
		{
			struct sg_source * si;

			si = &mgr->source_list[input_index++];

			if ( sg_source_init(si, i, j, dlr->entry[i].name,
					ilr->entry[j].name) )
			{
				ret = -1;
				goto bail_list;
			}
		}
	}

bail_list:

	if ( (err = SGDisposeDeviceList(src.grabber, device_list)) )
	{
		log_error("failed SGDisposeDeviceList() %d\n", err);
		return -1;
	}

bail_source:

	if ( sg_source_release(&src) )
		return -1;

	if ( sg_source_destroy(&src) )
		return -1;

	return ret;
}

int sg_manager_source_count(struct sg_manager * mgr)
{
	return mgr->source_list_len;
}

struct sg_source *
sg_manager_source_get(struct sg_manager * mgr, int index)
{
	if ( index >= mgr->source_list_len || index < 0 )
		return 0;

	return &mgr->source_list[index];
}

struct sg_source *
sg_manager_source_find(struct sg_manager * mgr, int device_id, int input_id)
{
	int i;

	for ( i = 0; i < mgr->source_list_len; ++i )
	{
		struct sg_source * src = &mgr->source_list[i];
		if ( sg_source_match(src, device_id, input_id) )
		{
			if ( src->acquired )
				log_warn("found acquired source %d,%d\n",
						device_id, input_id);
			return src;
		}
	}

	return 0;
}

struct sg_source *
sg_manager_source_default_find(struct sg_manager * mgr)
{
	int i;

	for ( i = 0; i < mgr->source_list_len; ++i )
	{
		struct sg_source * src = &mgr->source_list[i];

		if ( !src->acquired )
			return src;
	}

	return 0;
}

