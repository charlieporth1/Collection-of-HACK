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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#include "logging.h"

#ifndef HAVE_SNPRINTF
#define snprintf _snprintf
#endif

static FILE * logging_file = 0;

static enum log_level logging_level = log_level_info;

static void do_log(enum log_level level, const char * fmt, va_list ap)
{
	const char * level_string;
	char str[4096];

	if ( level > logging_level )
		return;

	switch ( level )
	{
	case log_level_error:
		level_string = "error";
		break;
	case log_level_warn:
		level_string = "warning";
		break;
	case log_level_info:
		level_string = "info";
		break;
	case log_level_debug:
		level_string = "debug";
		break;
	default:
		level_string = "unknown";
	}

	snprintf(str, sizeof(str), "vidcap: %s: %s", level_string, fmt);
	vfprintf(logging_file ? logging_file : stderr, str, ap);
}

void
log_file_set(FILE * f)
{
	logging_file = f;
}

void
log_level_set(enum log_level level)
{
	logging_level = level;
}

void log_error(const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	do_log(log_level_error, fmt, ap);
	va_end(ap);
}

void log_warn(const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	do_log(log_level_warn, fmt, ap);
	va_end(ap);
}

void log_info(const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	do_log(log_level_info, fmt, ap);
	va_end(ap);
}

void log_debug(const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	do_log(log_level_debug, fmt, ap);
	va_end(ap);
}

void log_oom(const char * file, int line)
{
	log_error("out of memory at %s:%d\n", file, line);
}
