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

#ifndef _DOUBLE_BUFFER_H
#define _DOUBLE_BUFFER_H

#include "os_funcs.h"

#define _DOUBLE_MEANS_TWO_ 2

struct double_buffer
{
	int read_count;
	int write_count;
	void * objects[_DOUBLE_MEANS_TWO_];
	vc_mutex locks[_DOUBLE_MEANS_TWO_];

	int count[_DOUBLE_MEANS_TWO_];

	void (*copy_object)(void *, const void *);

	/* TODO: remove */
	int num_insert_too_far_failures;
};

struct double_buffer *
double_buffer_create(void (*copy_object)(void *, const void *), void *, void *);

void
double_buffer_destroy(struct double_buffer * db_buff);

void
double_buffer_write(struct double_buffer * db_buff, const void * object);

int
double_buffer_read(struct double_buffer * db_buff, void * object);

int
double_buffer_count(struct double_buffer * db_buff);

#endif
