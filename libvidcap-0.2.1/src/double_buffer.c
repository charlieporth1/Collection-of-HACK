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

#include "double_buffer.h"

#include "logging.h"

/* The write thread inserts (and frees) objects.
 * The read thread reads (copies) objects
 *
 * It is the responsibility of the read thread's 'application' to
 * free the copied objects.
 *
 * If the read thread 'application' blocks for too long (doesn't 
 * read often enough), the write thread will free a buffered object
 * to make room for an incoming object
 */

/* NOTE: This function is passed a function pointer to
 *       call when necessary to copy an object
 */
struct double_buffer *
double_buffer_create( void (*copy_func)(void *, const void *), void *object1, void *object2 )
{
	struct double_buffer * db_buff;
		
	if ( !copy_func )
		return 0;

	db_buff = calloc(1, sizeof(*db_buff));

	if ( !db_buff )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	db_buff->read_count = 0;
	db_buff->write_count = 0;
	db_buff->count[0] = -1;
	db_buff->count[1] = -1;
	db_buff->objects[0] = object1;
	db_buff->objects[1] = object2;

	db_buff->copy_object = copy_func;

	vc_mutex_init(&db_buff->locks[0]);
	vc_mutex_init(&db_buff->locks[1]);

	/* TODO: remove this (debug) counter */
	db_buff->num_insert_too_far_failures       = 0;

	return db_buff;
}

void
double_buffer_destroy(struct double_buffer * db_buff)
{
	vc_mutex_destroy(&db_buff->locks[1]);
	vc_mutex_destroy(&db_buff->locks[0]);

	log_debug("Double buffer had counter reading of %d\n",
			db_buff->num_insert_too_far_failures);

	free(db_buff);
}

void
double_buffer_write(struct double_buffer * db_buff, const void * new_object)
{
	const int insertion_index = db_buff->write_count % 2;

	/* don't get far ahead of the reader */
	if ( db_buff->write_count > ( db_buff->read_count + 2 ) )
	{
		db_buff->num_insert_too_far_failures++;
		/* return; */
	}

	/* get exclusive access to the correct buffer */
	if ( vc_mutex_trylock(&db_buff->locks[insertion_index] ))
	{
		/* failed to obtain lock */
		/* drop incoming object  */
		log_info("callback is skipping a frame\n");
		return;
	}

	/* copy object */
	db_buff->copy_object(db_buff->objects[insertion_index], new_object);

	/* stamp the buffer with the write count */
	db_buff->count[insertion_index] = db_buff->write_count;

	/* advance the write count */
	++db_buff->write_count;

	/* unlock the correct lock */
	vc_mutex_unlock(&db_buff->locks[insertion_index]);
}

int
double_buffer_read(struct double_buffer * db_buff, void *dest_buffer)
{
	int copy_index = db_buff->read_count % 2;

	if ( db_buff->write_count < 1 )
		return -1;

	/* try the next buffer */
	if ( db_buff->count[copy_index] < db_buff->read_count )
	{
		/* Next buffer isn't ready. Use the previous buffer */
		copy_index = 1 - copy_index;
	}

	if ( vc_mutex_trylock(&db_buff->locks[copy_index] ) )
	{
		/* Failed to obtain buffer's lock.
		 * Try the other buffer?
		 */
		if ( db_buff->write_count < 2 )
			return -1;

		copy_index = 1 - copy_index;

		if ( db_buff->count[copy_index] < db_buff->read_count )
		{
			/* Too old. Don't read this buffer */
			vc_mutex_unlock(&db_buff->locks[copy_index]);
			log_info("Capture timer thread failed to obtain lock\n");
			return -1;
		}

		/* Try other lock. Failure should be rare */
		if ( vc_mutex_trylock(&db_buff->locks[copy_index] ) )
		{
			log_info("Capture timer thread failed to obtain 2nd lock\n");
			return -1;
		}
	}

	/* Is this buffer older than the last-read? */
	if ( db_buff->count[copy_index] < (db_buff->read_count - 1))
	{
		/* vidcap buffer is too old. Don't read it.
		 * This needs to be rare.
		 */
		vc_mutex_unlock(&db_buff->locks[copy_index]);
		log_info("Capture timer thread won't read stale buffer\n");
		return -1;
	}

	db_buff->copy_object(dest_buffer, db_buff->objects[copy_index]);

	db_buff->read_count = db_buff->count[copy_index] + 1;

	vc_mutex_unlock(&db_buff->locks[copy_index]);

	return 0;
}

int
double_buffer_count(struct double_buffer * db_buff)
{
	return db_buff->write_count;
}
