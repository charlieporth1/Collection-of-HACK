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
#include "sliding_window.h"

struct sliding_window *
sliding_window_create(int window_len, int object_size)
{
	struct sliding_window * swin = calloc(1, sizeof(*swin));

	if ( !swin )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	swin->objects = calloc(window_len + 1, object_size);

	if ( !swin->objects )
	{
		log_oom(__FILE__, __LINE__);
		free(swin);
		return 0;
	}

	swin->window_len = window_len;
	swin->object_size = object_size;
	swin->head = -1;
	swin->tail = 0;
	swin->count = 0;

	return swin;
}

int
sliding_window_count(struct sliding_window * swin)
{
	return swin->count;
}

void
sliding_window_destroy(struct sliding_window * swin)
{
	free(swin->objects);
	free(swin);
}

void
sliding_window_slide(struct sliding_window * swin, void * new_object)
{
	swin->head = ++swin->head % swin->window_len;

	memcpy(&swin->objects[swin->head * swin->object_size],
			new_object, swin->object_size);

	if ( swin->count < swin->window_len )
		++swin->count;
	else
		swin->tail = ++swin->tail % swin->window_len;
}

void *
sliding_window_peek_front(struct sliding_window * swin)
{
	if ( swin->count == 0 )
		return 0;

	return &swin->objects[swin->tail * swin->object_size];
}
