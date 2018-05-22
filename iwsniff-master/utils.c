/*
 *  Copyright (c) 2004 Claes M. Nyberg <md0claes@mdstud.chalmers.se>
 *  All rights reserved, all wrongs reversed.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: utils.c,v 1.4 2004-07-31 22:48:43 cmn Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "iwsniff.h"

extern struct options opt;

/*
 * Only print if vergose level is high enough
 */
void
vprint(int level, char *fmt, ...)
{
	va_list ap;

	if (opt.verbose < level)
		return;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	
}


/*
 * Warn
 */
void
warn(const char *fmt, ...)
{
    va_list ap;

	fprintf(stderr, "** Warning: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/*
 * Print to standard error
 */
void
err(const char *fmt, ...)
{
	va_list ap;
	
	fprintf(stderr, "** Error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


/*
 * Print to standard error and exit
 */
void
errx(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "** Error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


/*
 * Allocate memory or die
 */
void *
memx(size_t n)
{
	void *pt;
	if ( (pt = malloc(n)) == NULL)
		errx("malloc(): %s", strerror(errno));
	return(pt);
}

/*
 * Allocate and zero out memory or exit
 */
void *
zmemx(size_t n)
{
	void *pt;
	pt = memx(n);
	memset(pt, 0x00, n);
	return(pt);
}


/*
 * Returns time in seconds as a string
 */
char *
currtime(time_t caltime)
{
    static u_char tstr[256];
    struct tm *tm;

    memset(tstr, 0x00, sizeof(tstr));

    if  ( (tm = localtime(&caltime)) == NULL) {
        snprintf(tstr, sizeof(tstr) -1, "localtime(): %s",
                strerror(errno));
    }

    else if (strftime(tstr, sizeof(tstr) -1, "%Y-%m-%d %H:%M:%S", tm) == 0) {
        snprintf(tstr, sizeof(tstr) -1, "localtime(): %s",
            strerror(errno));
    }

    return(tstr);
}

/*
 * Returns 1 if path is a regular file, 0 otherwise
 */
int
is_reg_file(const char *path)
{
	struct stat sb;

	if (path == NULL)
		return(0);

	memset(&sb, 0x00, sizeof(sb));
	if (stat(path, &sb) < 0)
		return(0);
		
	return(S_ISREG(sb.st_mode));
}

/*
 *  Returns size of regular file in bytes, -1 on error
 */
int
file_size(const char *path)
{
	struct stat sb;

	if ((path == NULL) || !is_reg_file(path))
		return(-1);
	
	memset(&sb, 0x00, sizeof(sb));
	if (stat(path, &sb) < 0)
		return(-1);
	
	return(sb.st_size);
}


/*
 * Locate bytes in buffer.
 * Returns index to start of str in buf on success,
 * -1 if string was not found.
 */
ssize_t
memstr(void *big, size_t blen, const void *little, size_t llen)
{
    ssize_t i = 0;
 
    if (blen < llen)
        return(-1); 
 
    while ( (blen - i) >= llen) {
        if (memcmp((u_char *)big + i, little, llen) == 0)
            return(i);
        i++;
    }
 
    return(-1);
}       

/*
 * Join strings in argv together with str as separator.
 * Returns a string which has to be free'd using free
 * on success, NULL otherwise.
 */
char *
str_join(const char *str, char **argv)
{
	char *js = NULL;
	size_t n;
	size_t i;
	size_t j;

	if ((str == NULL) || (argv == NULL) || (argv[0] == NULL)) 
		return(NULL);
	
	j = strlen(str);

	/* Count bytes needed */
	for (n=0, i=0; argv[i] != NULL; i++)
		n += strlen(argv[i]) + j;

	if (n == 0)
		return(NULL);

	/* Just pad with a few bytes */
	n += 24;
	js = zmemx(n);

	/* Join strings together */
	for (j=0, i=0; argv[i] != NULL; i++) {
		
		/* Last string, don't append separator */
		if (argv[i+1] == NULL)
			j += snprintf(&js[j], n-j, "%s", argv[i]);
		else
			j += snprintf(&js[j], n-j, "%s%s", argv[i], str);
	}

	return(js);
}
