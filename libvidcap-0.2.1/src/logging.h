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

#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum log_level
{
	log_level_none  = 0,
	log_level_error = 1,
	log_level_warn  = 2,
	log_level_info  = 3,
	log_level_debug = 4
};

void log_file_set(FILE *);
void log_level_set(enum log_level);

#ifdef __GNUC__
void log_error(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
void log_warn(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
void log_info(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
void log_debug(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
void log_error(const char * fmt, ...);
void log_warn(const char * fmt, ...);
void log_info(const char * fmt, ...);
void log_debug(const char * fmt, ...);
#endif

void log_oom(const char * file, int line);

#ifdef __cplusplus
}
#endif

#endif
