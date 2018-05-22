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
 * $Id: daemon.c,v 1.2 2004-07-31 19:24:56 cmn Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Become daemon.
 * Returns -1 on error, 0 otherwise
 */
int
daemonize(void)
{

    /* Fork first time to (among other things) guarantee that
     * we are not a process group leader */
    switch(fork()) {
        case -1:
            perror("fork");
            return(-1);
            break;

        case 0: /* Child continues */
            break;

        default:
            exit(EXIT_FAILURE);
    }

    /* Become a process group and session group leader */
    if (setsid() < 0) {
        perror("setsid");
        return(-1);
    }

    /* Fork again so that the parent (session leader) can exit
     * (we dont want a controlling terminal) */
    switch(fork()) {
        case -1:
            perror("fork");
            exit(-1);
            break;

        case 0: /* Nice */
            break;

        default: /* Exit session leader */
            exit(0);
            break;
    }

	/* Ignore some signals */
	signal(SIGHUP, SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);

    /* Do not block any directories */
    // chdir("/");

    /* Set umask */
    umask(0077);

    return(0);
}

