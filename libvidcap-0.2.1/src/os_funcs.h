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

#ifndef _OS_FUNCS_H
#define _OS_FUNCS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_NANOSLEEP) || defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#include <time.h>
#endif

/* os-dependent macros */
#if defined(WIN32) || defined(_WIN32_WCE)

#ifndef _WIN32_WINNT
// Change this to the appropriate value to target other versions of Windows.
// see: http://msdn2.microsoft.com/en-us/library/aa383745.aspx
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>

#if !defined(_WIN32_WCE)
#include <process.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#define STDCALL	__stdcall
typedef CRITICAL_SECTION vc_mutex;
typedef uintptr_t vc_thread;

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>

#define STDCALL
typedef pthread_mutex_t vc_mutex;
typedef pthread_t vc_thread;

#endif

#ifdef MACOSX
#include <mach/mach_init.h>
#include <mach/thread_policy.h>
#include <sched.h>
#include <sys/sysctl.h>
#endif

static __inline struct timeval
vc_now(void)
{
	struct timeval tv;
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&tv, 0);
#elif defined(WIN32)
	FILETIME ft;
	LARGE_INTEGER li;
	__int64 t;
	static int tzflag;
	const __int64 EPOCHFILETIME = 116444736000000000i64;

	GetSystemTimeAsFileTime(&ft);
	li.LowPart  = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	t  = li.QuadPart;       /* In 100-nanosecond intervals */
	t -= EPOCHFILETIME;     /* Offset to the Epoch time */
	t /= 10;                /* In microseconds */
	tv.tv_sec  = (long)(t / 1000000);
	tv.tv_usec = (long)(t % 1000000);
#else
#error no gettimeofday or equivalent available
#endif
	return tv;
}

static __inline int
vc_create_thread(vc_thread *thread,
		unsigned int (STDCALL * thread_func)(void *),
		void *args, unsigned int *thread_id)
{
	int ret = 0;
#ifdef WIN32
	*thread = (uintptr_t)_beginthreadex(NULL, 0, thread_func,
			(void *)args, 0, thread_id);

	/* FIXME: log the error */
	if ( thread == 0 )
		ret = errno;
#else
	void * (*func)(void *) = (void * (*)(void *))thread_func;

	ret = pthread_create(thread, NULL, func, args);

	/* FIXME: log an error */
#endif

	return ret;
}

static __inline int
vc_thread_join(vc_thread *thread)
{
#ifdef WIN32
	HANDLE _thread = (HANDLE)thread;

	DWORD ret = WaitForSingleObject(_thread, INFINITE);

	if ( ret == WAIT_FAILED )
		return GetLastError();

	return 0;
#else
	pthread_t *_thread = (pthread_t *)thread;

	return pthread_join(*_thread, 0);
#endif
}

static __inline int
vc_mutex_init(vc_mutex *m)
{
#ifdef WIN32
	InitializeCriticalSection(m);

	return 0;
#else
	return pthread_mutex_init(m, NULL);
#endif
}

static __inline int
vc_mutex_lock(vc_mutex *m)
{
#ifdef WIN32
	EnterCriticalSection(m);

	return 0;
#else
	return pthread_mutex_lock(m);
#endif
}

static __inline int
vc_mutex_trylock(vc_mutex *m)
{
#ifdef WIN32
	return !TryEnterCriticalSection(m);
#else
	return pthread_mutex_trylock(m);
#endif
}

static __inline int
vc_mutex_unlock(vc_mutex *m)
{
#ifdef WIN32
	LeaveCriticalSection(m);

	return 0;
#else
	return pthread_mutex_unlock(m);
#endif
}

static __inline int
vc_mutex_destroy(vc_mutex *m)
{
#ifdef WIN32
	DeleteCriticalSection(m);

	return 0;
#else
	return pthread_mutex_destroy(m);
#endif
}

static __inline void
vc_millisleep(long ms)
{
#ifdef WIN32

	Sleep(ms);

#else

	struct timespec req;

	req.tv_nsec = (ms % 1000) * 1000000;
	req.tv_sec = ms / 1000;

	nanosleep(&req, NULL);

#endif
}

#endif
