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
 * $Id: net.c,v 1.3 2004-07-31 19:24:56 cmn Exp $
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include "net.h"

/*
 * Get official name of service as a string 
 * by port number (network byte order).
 */
char *
service_tcp_port(u_short port)
{
	struct servent *sent;
	if ( (sent = getservbyport(port, "tcp")) != NULL)
		return(sent->s_name);
	return(NULL);
}


/*
 * Get official port (network byte order) of service
 * given the name as a string.
 * Returns -1 if no service was found.
 */  
int 
service_tcp_name(const char *name)
{
	struct servent *sent;
	if ( (sent = getservbyname(name, "tcp")) != NULL)
		return(sent->s_port);
	return(-1);
}

/*
 * Translate network byte ordered IP address into its
 * dotted decimal representation.
 */
char *
net_ntoa(u_long ip)
{
    struct in_addr ipa;
    ipa.s_addr = ip;
    return(inet_ntoa(ipa));
}

/*
 * Returns official name of host from network byte
 * ordered IP address on success, NULL on error.
 */
char *
net_hostname(struct in_addr *addr)
{
    static u_char hname[MAXHOSTNAMELEN+1];
    struct hostent *hent;

    if ( (hent = gethostbyaddr((char *)&(addr->s_addr),
            sizeof(addr->s_addr), AF_INET)) == NULL) {
        return(NULL);
    }
    snprintf(hname, sizeof(hname) -1, "%s", hent->h_name);
    return(hname);
}

/*
 * Returns official name of host from network byte
 * ordered IP address on success, NULL on error.
 */
char *
net_hostname2(u_int ip)
{
    struct in_addr ipa;
    ipa.s_addr = ip;
    return(net_hostname(&ipa));
}

/*
 * Translate hostname or dotted decimal host address
 * into a network byte ordered IP address.
 * Returns -1 on error.
 */
long
net_inetaddr(u_char *host)
{
    long haddr;
    struct hostent *hent;

    if ( (haddr = inet_addr(host)) < 0) {
        if ( (hent = gethostbyname(host)) == NULL)
            return(-1);

        memcpy(&haddr, (hent->h_addr), 4);
    }
    return(haddr);
}

/*
 * Convert 6 byte MAC address to string
 */
u_char *
macstr(u_char *mac)
{
	static u_char mstr[48];

	memset(mstr, 0x00, sizeof(mstr));
	snprintf(mstr, sizeof(mstr)-1, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return(mstr);
}
