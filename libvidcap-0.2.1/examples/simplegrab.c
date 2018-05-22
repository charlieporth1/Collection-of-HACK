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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#ifndef HAVE_SNPRINTF
#define snprintf _snprintf
#endif
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_NANOSLEEP) || defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#include <time.h>
#endif

#include <vidcap/vidcap.h>

static int ctrl_c_pressed = 0;

static int opt_quiet = 0;
static int opt_do_enumeration = 0;
static int opt_do_defaults = 0;
static int opt_do_double_default = 0;
static int opt_do_captures = 0;
static int opt_do_notifies = 0;

struct my_source_context
{
	vidcap_src * src;
	char name[VIDCAP_NAME_LENGTH];
	int capture_count;
	struct timeval tv0;
	struct timeval tv1;
};

#ifndef HAVE_GETTIMEOFDAY
__inline int gettimeofday(struct timeval * tv, struct timezone * tz)
#ifdef WIN32
{
	FILETIME ft;
	LARGE_INTEGER li;
	__int64 t;
	static int tzflag;
	const __int64 EPOCHFILETIME = 116444736000000000i64;
	if ( tv )
	{
		GetSystemTimeAsFileTime(&ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tv->tv_sec  = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
	}

	return 0;

#else /* !defined(_WINDOWS) */
	errno = ENOSYS;
	return -1;
#endif
}
#endif

static int
timeval_subtract(struct timeval * result,
		struct timeval * t1, struct timeval * t0)
{
	struct timeval t0t = *t0;

	/* Perform the carry for the later subtraction by updating t0t. */
	if ( t1->tv_usec < t0t.tv_usec )
	{
		int nsec = (t0t.tv_usec - t1->tv_usec) / 1000000 + 1;
		t0t.tv_usec -= 1000000 * nsec;
		t0t.tv_sec += nsec;
	}

	if ( t1->tv_usec - t0t.tv_usec > 1000000 )
	{
		int nsec = (t1->tv_usec - t0t.tv_usec) / 1000000;
		t0t.tv_usec += 1000000 * nsec;
		t0t.tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	   tv_usec is certainly positive. */
	result->tv_sec = t1->tv_sec - t0t.tv_sec;
	result->tv_usec = t1->tv_usec - t0t.tv_usec;

	/* Return 1 if result is negative. */
	return t1->tv_sec < t0t.tv_sec;
}

static void my_sleep_ms(int milliseconds)
{
#if defined(HAVE_NANOSLEEP)
	struct timespec tv;

	tv.tv_sec = milliseconds / 1000;
	tv.tv_nsec = milliseconds % 1000 * 1000000;

	nanosleep(&tv, 0);

#elif defined(_WIN32)
	Sleep(milliseconds);
#else
#error No sleep function available.
#endif
}

static void mylog(const char * level_string, const char * fmt, va_list ap)
{
	fprintf(stderr, "simplegrab: %s: ", level_string);
	vfprintf(stderr, fmt, ap);
}

static char * my_fmt_info_str(struct vidcap_fmt_info * fmt_info)
{
	const int size = 255;
	char * s = malloc(size);
	snprintf(s, size, "%3dx%3d %s %.2f %d/%d",
			fmt_info->width,
			fmt_info->height,
			vidcap_fourcc_string_get(fmt_info->fourcc),
			(float)fmt_info->fps_numerator /
			(float)fmt_info->fps_denominator,
			fmt_info->fps_numerator,
			fmt_info->fps_denominator);
	return s;
}

#ifdef DEBUG
static void log_debug(char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	mylog("debug", fmt, ap);
	va_end(ap);
}
#define log_debug(...)
#endif

static void log_info(char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	mylog("info", fmt, ap);
	va_end(ap);
}

static void log_warn(char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	mylog("warn", fmt, ap);
	va_end(ap);
}

static void log_error(char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	mylog("error", fmt, ap);
	va_end(ap);
}

static void signal_handler(int s)
{
	ctrl_c_pressed = 1;
}

static void usage(void)
{
	fprintf(stderr, "Usage: simplegrab [OPTIONS]\n"
			" -h  -- show this help message\n"
			" -q  -- decrease libvidcap verbosity\n"
			" -e  -- do enumeration test\n"
			" -d  -- do defaults test\n"
			" -2  -- do double default test\n"
			" -c  -- do capture test\n"
			" -n  -- do notification test\n");
}

static int process_command_line(int argc, char * argv[])
{
	int i;

	for ( i = 1; i < argc; ++i )
	{
		if ( !strcmp(argv[i], "-e") )
			opt_do_enumeration = 1;
		else if ( !strcmp(argv[i], "-d") )
			opt_do_defaults = 1;
		else if ( !strcmp(argv[i], "-2") )
			opt_do_double_default = 1;
		else if ( !strcmp(argv[i], "-c") )
			opt_do_captures = 1;
		else if ( !strcmp(argv[i], "-n") )
			opt_do_notifies = 1;
		else if ( !strcmp(argv[i], "-q") )
			opt_quiet += 1;
		else if ( !strcmp(argv[i], "-h") )
		{
			usage();
			exit(0);
		}
		else
		{
			log_error("unknown option '%s'\n", argv[i]);
			return -1;
		}
	}

	/* If none of the tests are explicitly set, then we go ahead
	 * and do all of them.
	 */
	if ( !(opt_do_enumeration ||
			opt_do_defaults ||
			opt_do_double_default ||
			opt_do_captures ||
			opt_do_notifies) )
	{
		opt_do_enumeration = 1;
		opt_do_defaults = 1;
		opt_do_double_default = 1;
		opt_do_captures = 1;
		opt_do_notifies = 1;
	}

	return 0;
}

static int user_capture_callback(vidcap_src * src,
		void * user_data,
		struct vidcap_capture_info * cap_info)
{
	struct my_source_context * ctx = user_data;

	if ( ctx->capture_count == 0 )
		gettimeofday(&ctx->tv0, 0);

	gettimeofday(&ctx->tv1, 0);

	ctx->capture_count++;

	return 0;
}

static int user_notification_callback(vidcap_sapi * sapi, void * user_data)
{
	struct vidcap_src_info * src_list;
	int src_list_len;
	int i;

	log_info("\n");
	log_info("notification...\n");

	src_list_len = vidcap_src_list_update(sapi);

	if ( src_list_len < 0 )
	{
		log_error("failed vidcap_src_list_update()\n");
		return -1;
	}

	if ( !(src_list = calloc(src_list_len,
					sizeof(struct vidcap_src_info))) )
	{
		log_error("out of memory");
		return -1;
	}

	if ( vidcap_src_list_get(sapi, src_list_len, src_list) )
	{
		free(src_list);
		log_error("failed vidcap_src_list_get()\n");
		return -1;
	}

	for ( i = 0; i < src_list_len; ++i )
	{
		log_info(" src: %2d | %s | %s\n", i,
				src_list[i].identifier,
				src_list[i].description);
	}

	free(src_list);

	return 0;
}

static int
do_enumeration()
{
	vidcap_state * vc;
	struct vidcap_sapi_info sapi_info;
	int i;

	log_info("Starting enumeration\n");

	if ( !(vc = vidcap_initialize()) )
	{
		log_error("failed vidcap_initialize()\n");
		return -1;
	}

	for ( i = 0; vidcap_sapi_enumerate(vc, i, &sapi_info); ++i )
	{
		vidcap_sapi * sapi;
		struct vidcap_src_info * src_list;
		int src_list_len;
		int j;

		log_info("sapi: %2d | %s | %s\n", i,
				sapi_info.identifier,
				sapi_info.description);

		if ( !(sapi = vidcap_sapi_acquire(vc, &sapi_info)) )
		{
			log_error("failed to acquire sapi\n");
			return -1;
		}

		if ( (src_list_len = vidcap_src_list_update(sapi)) < 0 )
		{
			log_error("failed to update sapi source list\n");
			return -1;
		}

		if ( !(src_list = calloc(src_list_len,
				sizeof(struct vidcap_src_info))) )
		{
			log_error("out of memory\n");
			return -1;
		}

		if ( vidcap_src_list_get(sapi, src_list_len, src_list) )
		{
			log_error("failed to get source list\n");
			return -1;
		}

		for ( j = 0; j < src_list_len; ++j )
		{
			vidcap_src * src;
			struct vidcap_fmt_info fmt_info;
			int k;

			log_info(" src: %2d | %s | %s\n", j,
					src_list[j].identifier,
					src_list[j].description);

			if ( !(src = vidcap_src_acquire(sapi, &src_list[j])) )
			{
				log_warn("failed to acquire %d %s\n", j,
						src_list[j].identifier);
				continue;
			}

			for ( k = 0; vidcap_format_enumerate(src, k, &fmt_info);
					++k )
			{
				char * fmt_str = my_fmt_info_str(&fmt_info);
				log_info(" fmt: %2d %s\n", k, fmt_str);
				free(fmt_str);

				if ( vidcap_format_bind(src, &fmt_info) )
				{
					log_warn("failed to bind format\n");
					break;
				}
			}

			if ( vidcap_src_release(src) )
			{
				log_warn("failed to release %d %s\n", j,
						src_list[j].identifier);
			}
		}

		free(src_list);

		if ( vidcap_sapi_release(sapi) )
		{
			log_error("failed to release sapi\n");
			return -1;
		}
	}

	vidcap_destroy(vc);

	log_info("Finished enumeration\n\n");

	return 0;
}

static int
do_defaults()
{
	vidcap_state * vc;
	vidcap_sapi * sapi;
	vidcap_src * src;

	struct vidcap_sapi_info sapi_info;
	struct vidcap_src_info src_info;
	struct vidcap_fmt_info fmt_info;

	char * fmt_str;

	log_info("Starting defaults\n");

	if ( !(vc = vidcap_initialize()) )
	{
		log_error("failed vidcap_initialize()\n");
		return -1;
	}

	if ( !(sapi = vidcap_sapi_acquire(vc, 0)) )
	{
		log_error("failed to acquire default sapi\n");
		return -1;
	}

	if ( vidcap_sapi_info_get(sapi, &sapi_info) )
	{
		log_error("failed to get default sapi info\n");
		return -1;
	}

	log_info("sapi default: %s | %s\n", sapi_info.identifier,
			sapi_info.description);

	if ( !(src = vidcap_src_acquire(sapi, 0)) )
	{
		log_error("failed to acquire default src\n");
		return -1;
	}

	if ( vidcap_src_info_get(src, &src_info) )
	{
		log_error("failed to get default src info\n");
		return -1;
	}

	log_info(" src default: %s | %s\n", src_info.identifier,
			src_info.description);

	if ( vidcap_format_bind(src, 0) )
	{
		log_error("failed to bind default format\n");
		return -1;
	}

	if ( vidcap_format_info_get(src, &fmt_info) )
	{
		log_error("failed to get default fmt info\n");
		return -1;
	}

	fmt_str = my_fmt_info_str(&fmt_info);
	log_info(" fmt default: %s\n", fmt_str);
	free(fmt_str);

	if ( vidcap_src_release(src) )
	{
		log_error("failed to release src\n");
		return -1;
	}

	if ( vidcap_sapi_release(sapi) )
	{
		log_error("failed to release sapi\n");
		return -1;
	}

	vidcap_destroy(vc);

	log_info("Finished defaults\n\n");

	return 0;
}

static int
do_double_defaults()
{
	vidcap_state * vc;
	vidcap_sapi * sapi;
	vidcap_src * src;
	vidcap_src * src2;

	struct vidcap_sapi_info sapi_info;
	struct vidcap_src_info src_info;
	struct vidcap_src_info src_info2;
	struct vidcap_fmt_info fmt_info;
	struct vidcap_fmt_info fmt_info2;

	char * fmt_str;

	log_info("Starting defaults\n");

	if ( !(vc = vidcap_initialize()) )
	{
		log_error("failed vidcap_initialize()\n");
		return -1;
	}

	if ( !(sapi = vidcap_sapi_acquire(vc, 0)) )
	{
		log_error("failed to acquire default sapi\n");
		return -1;
	}

	if ( vidcap_sapi_info_get(sapi, &sapi_info) )
	{
		log_error("failed to get default sapi info\n");
		return -1;
	}

	log_info("sapi default: %s | %s\n", sapi_info.identifier,
			sapi_info.description);

	if ( !(src = vidcap_src_acquire(sapi, 0)) )
	{
		log_error("failed to acquire default src\n");
		return -1;
	}

	if ( vidcap_src_info_get(src, &src_info) )
	{
		log_error("failed to get default src info\n");
		return -1;
	}

	log_info(" src default: %s | %s\n", src_info.identifier,
			src_info.description);

	if ( vidcap_format_bind(src, 0) )
	{
		log_error("failed to bind default format\n");
		return -1;
	}

	if ( vidcap_format_info_get(src, &fmt_info) )
	{
		log_error("failed to get default fmt info\n");
		return -1;
	}

	fmt_str = my_fmt_info_str(&fmt_info);
	log_info(" fmt default: %s\n", fmt_str);
	free(fmt_str);

	/* Acquire a second default (with first still acquired) */
	if ( !(src2 = vidcap_src_acquire(sapi, 0)) )
	{
		log_error("failed to acquire default src 2\n");
		return -1;
	}

	if ( vidcap_src_info_get(src2, &src_info2) )
	{
		log_error("failed to get default src info 2\n");
		return -1;
	}

	log_info(" src default 2: %s | %s\n", src_info2.identifier,
			src_info2.description);

	if ( vidcap_format_bind(src2, 0) )
	{
		log_error("failed to bind default format 2\n");
		return -1;
	}

	if ( vidcap_format_info_get(src2, &fmt_info2) )
	{
		log_error("failed to get default fmt info 2\n");
		return -1;
	}

	fmt_str = my_fmt_info_str(&fmt_info2);
	log_info(" fmt default 2: %s\n", fmt_str);
	free(fmt_str);

	if ( vidcap_src_release(src) )
	{
		log_error("failed to release src\n");
		return -1;
	}

	if ( vidcap_src_release(src2) )
	{
		log_error("failed to release src 2\n");
		return -1;
	}

	if ( vidcap_sapi_release(sapi) )
	{
		log_error("failed to release sapi\n");
		return -1;
	}

	vidcap_destroy(vc);

	log_info("Finished defaults\n\n");

	return 0;
}

static int
do_captures()
{
	const int sleep_ms = 10000;
	vidcap_state * vc;
	vidcap_sapi * sapi;

	struct vidcap_sapi_info sapi_info;
	struct vidcap_src_info * src_list;
	int src_list_len;

	struct my_source_context * ctx_list;

	int i;

	log_info("Starting captures\n");

	if ( !(vc = vidcap_initialize()) )
	{
		log_error("failed vidcap_initialize()\n");
		return -1;
	}

	if ( !(sapi = vidcap_sapi_acquire(vc, 0)) )
	{
		log_error("failed to acquire default sapi\n");
		return -1;
	}

	if ( vidcap_sapi_info_get(sapi, &sapi_info) )
	{
		log_error("failed to get default sapi info\n");
		return -1;
	}

	log_info("sapi: %s | %s\n", sapi_info.identifier,
			sapi_info.description);

	src_list_len = vidcap_src_list_update(sapi);

	if ( src_list_len < 0 )
	{
		log_error("failed vidcap_src_list_update()\n");
		return -1;
	}
	else if ( src_list_len == 0 )
	{
		log_error("no sources available\n");
		return -1;
	}

	if ( !(src_list = calloc(src_list_len,
					sizeof(struct vidcap_src_info))) )
	{
		log_error("out of memory\n");
		return -1;
	}

	if ( vidcap_src_list_get(sapi, src_list_len, src_list) )
	{
		log_error("failed vidcap_src_list_get()\n");
		return -1;
	}

	if ( !(ctx_list = calloc(src_list_len, sizeof(*ctx_list))) )
	{
		log_error("out of memory\n");
		return -1;
	}

	for ( i = 0; i < src_list_len; ++i )
	{
		struct vidcap_fmt_info fmt_info;
		char * fmt_str;

		log_info(" src: %2d | %s | %s\n", i,
				src_list[i].identifier,
				src_list[i].description);

		if ( !(ctx_list[i].src = vidcap_src_acquire(sapi,
						&src_list[i])) )
		{
			log_error("failed vidcap_src_acquire()\n");
			return -1;
		}

		if ( vidcap_format_bind(ctx_list[i].src, 0) )
		{
			log_error("failed vidcap_format_bind()\n");
			return -1;
		}

		if ( vidcap_format_info_get(ctx_list[i].src, &fmt_info) )
		{
			log_error("failed vidcap_format_info_get()\n");
			return -1;
		}

		fmt_str = my_fmt_info_str(&fmt_info);
		log_info(" fmt default: %s\n", fmt_str);
		free(fmt_str);

		sprintf(ctx_list[i].name, "source %d", i);

		if ( vidcap_src_capture_start(ctx_list[i].src,
					user_capture_callback,
					&ctx_list[i]) )
		{
			log_error("failed vidcap_src_capture_start()\n");
			return -1;
		}
	}

	free(src_list);

	log_info("capturing for %d seconds...\n", sleep_ms / 1000);
	my_sleep_ms(sleep_ms);

	for ( i = 0; i < src_list_len; ++i )
	{
		struct timeval tv_cap;
		double capture_time;

		if ( !ctx_list[i].src )
			continue;

		if ( vidcap_src_capture_stop(ctx_list[i].src) )
		{
			log_error("failed vidcap_src_capture_stop() %d\n", i);
			return -1;
		}

		timeval_subtract(&tv_cap, &ctx_list[i].tv1,
				&ctx_list[i].tv0);

		capture_time = (double)tv_cap.tv_sec +
			(double)tv_cap.tv_usec / 1000000.0;

		log_info("%s: %d callbacks in %.2lfs, %.2lf frames per second\n",
				ctx_list[i].name,
				ctx_list[i].capture_count,
				capture_time,
				(double)ctx_list[i].capture_count /
				capture_time);

		if ( vidcap_src_release(ctx_list[i].src) )
		{
			log_info("failed vidcap_src_release() %d\n", i);
			return -1;
		}
	}

	free(ctx_list);

	if ( vidcap_sapi_release(sapi) )
	{
		log_error("failed vidcap_sapi_release()\n");
		return -1;
	}

	vidcap_destroy(vc);

	log_info("Finished captures\n\n");

	return 0;
}

int
do_notifies()
{
	vidcap_state * vc;
	vidcap_sapi * sapi;
	struct vidcap_sapi_info sapi_info;
	struct vidcap_src_info * src_list;
	int src_list_len;
	int i;

	log_info("Starting notifies\n");

	signal(SIGINT, signal_handler);

	if ( !(vc = vidcap_initialize()) )
	{
		log_error("failed vidcap_initialize()\n");
		return -1;
	}

	if ( !(sapi = vidcap_sapi_acquire(vc, 0)) )
	{
		log_error("failed to acquire default sapi\n");
		return -1;
	}

	if ( vidcap_sapi_info_get(sapi, &sapi_info) )
	{
		log_error("failed to get default sapi info\n");
		return -1;
	}

	log_info("sapi: %s | %s\n", sapi_info.identifier,
			sapi_info.description);

	src_list_len = vidcap_src_list_update(sapi);

	if ( src_list_len < 0 )
	{
		log_error("failed vidcap_src_list_update()\n");
		return -1;
	}
	else if ( src_list_len )
	{
		if ( !(src_list = calloc(src_list_len,
					sizeof(struct vidcap_src_info))) )
		{
			log_error("out of memory\n");
			return -1;
		}

		if ( vidcap_src_list_get(sapi, src_list_len, src_list) )
		{
			log_error("failed vidcap_src_list_get()\n");
			return -1;
		}

		for ( i = 0; i < src_list_len; ++i )
		{
			log_info(" src: %2d | %s | %s\n", i,
					src_list[i].identifier,
					src_list[i].description);
		}

		free(src_list);
	}

	if ( vidcap_srcs_notify(sapi, user_notification_callback, 0) )
	{
		log_error("failed vidcap_srcs_notify()\n");
		return -1;
	}

	while ( !ctrl_c_pressed )
		my_sleep_ms(100);

	if ( vidcap_sapi_release(sapi) )
	{
		log_error("failed to release sapi\n");
		return -1;
	}

	vidcap_destroy(vc);

	log_info("Finished notifies\n\n");

	return 0;
}

int main(int argc, char * argv[])
{
	if ( process_command_line(argc, argv) )
	{
		usage();
		return 1;
	}

	if ( opt_quiet > 1 )
		vidcap_log_level_set(VIDCAP_LOG_WARN);
	else if ( opt_quiet > 0 )
		vidcap_log_level_set(VIDCAP_LOG_INFO);
	else
		vidcap_log_level_set(VIDCAP_LOG_DEBUG);

	if ( opt_do_enumeration && do_enumeration() )
		return 1;

	if ( opt_do_defaults && do_defaults() )
		return 1;

	if ( opt_do_double_default && do_double_defaults() )
		return 1;

	if ( opt_do_captures && do_captures() )
		return 1;

	if ( opt_do_notifies && do_notifies() )
		return 1;

	return 0;
}
