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
 * $Id: scan.c,v 1.8 2009-03-18 08:52:19 cmn Exp $
 */

#include <stdio.h>
#include <string.h>
#include "iwsniff.h"

extern struct options opt;

/* Scan routines */
struct scanfunc {
	int port;
	int (*func)(u_int8_t *, size_t, u_int8_t *, size_t);
	char *name;
};

/*
 * Crappy O(n) when scanning for service with a given port
 * TODO: Arrange services by port number in a binary tree 
 */

struct scanfunc tcp_funcs[] =
{
	{ 5190, decode_aim, "aim"},
	{ 9898, decode_aim, "aim"},
	{ 1494, decode_citrix, "citrix"},
	{ 2401, decode_cvs, "cvs"},
	{ 21, decode_ftp, "ftp"},
	{ 2121, decode_ftp, "ftp"},
//	{ , decode_hex, "hex"},
	{ 80, decode_http, "http"},
	{ 98, decode_http, "http"},
	{ 3128, decode_http, "http"},
	{ 8080, decode_http, "http"},
	{ 8000, decode_http, "http"},
	{ 4000, decode_icq, "icq"},
	{ 143, decode_imap, "imap"},
	{ 220, decode_imap, "imap"},
	{ 6667, decode_irc, "irc"},
	{ 6668, decode_irc, "irc"},
	{ 6669, decode_irc, "irc"},
	{ 389, decode_ldap, "ldap"},
	{ 417, decode_mmxp, "mmxp/tcp"},
	{ 2417, decode_mmxp, "mmxp/tcp"},
	{ 4444, decode_napster, "napster"},
	{ 5555, decode_napster, "napster"},
	{ 6666, decode_napster, "napster"},
	{ 7777, decode_napster, "napster"},
	{ 8888, decode_napster, "napster"},
	{ 119, decode_nntp, "nntp"},
	{ 1521, decode_oracle, "oracle"},
	{ 1526, decode_oracle, "oracle"},
	{ 5631, decode_pcanywhere, "pcanywhere"},
	{ 65301, decode_pcanywhere, "pcanywhere"},
	{ 106, decode_pop, "pop"},
	{ 109, decode_pop, "pop"},
	{ 110, decode_pop, "pop"},
	{ 5432, decode_postgresql, "postgresql"},
	{ 512, decode_rlogin, "rlogin"},
	{ 513, decode_rlogin, "rlogin"},
	{ 514, decode_rlogin, "rlogin"},
	{ 139, decode_smb, "smb"},
	{ 24, decode_smtp, "smtp"},
	{ 25, decode_smtp, "smtp"},
	{ 587, decode_smtp, "smtp"},
	{ 1080, decode_socks, "socks"},
	{ 1433, decode_tds, "tds/udp"},
	{ 2638, decode_tds, "tds/udp"},
	{ 7599, decode_tds, "tds/udp"},
	{ 23, decode_telnet, "telnet"},
	{ 261, decode_telnet, "telnet"},
	{ 6000, decode_x11, "x11"},
	{ 6001, decode_x11, "x11"},
	{ 6002, decode_x11, "x11"},
	{ 6003, decode_x11, "x11"},
	{ 6004, decode_x11, "x11"},
	{ 6005, decode_x11, "x11"},
	{-1, NULL}
};

struct scanfunc udp_funcs[] =
{
	{ 417, decode_mmxp, "mmxp/udp"},
	{ 2417, decode_mmxp, "mmxp/udp"},
	{ 520, decode_rip, "rip"},
	{ 2001, decode_sniffer, "sniffer"},
	{ 161, decode_snmp, "snmp"},
	{ 1433, decode_tds, "tds/udp"},
	{-1, NULL}	
};

struct scanfunc rpc_funcs[] =
{
	{ 100005, decode_mountd, "mountd"},
	{ 100004, decode_ypserv, "ypserv"},
	{ 100009, decode_yppasswd, "yppasswd"},
	{-1, NULL}
};

struct scanfunc ip_funcs[] =
{
	{ 89, decode_ospf, "ospf"},
	{ 47, decode_pptp, "pptp"},
	{ 112, decode_vrrp, "vrrp"},
	{-1, NULL}
};

void
print_banner(time_t ts, Wlan_nfo *wi, char *name, u_int saddr, 
		u_short sport, u_int daddr, u_short dport)
{
    printf("[%s] ", currtime(ts));

	if (wi && (opt.nobssid == 0))
		printf("BSSID:%s ", macstr(wi->bssid));

	printf("(%s) ", name);

	if (opt.logf)
		fprintf(opt.logf, "[%s] (%s) ", currtime(ts), name);

	if (opt.resolve) {
		char *pt;
		pt = net_hostname2(saddr);
		printf("%s.%u -> ", pt ? pt :net_ntoa(saddr), ntohs(sport));
		if (opt.logf)
			fprintf(opt.logf, "%s.%u -> ", pt ? pt :net_ntoa(saddr), ntohs(sport));
		pt = net_hostname2(daddr);
		printf("%s.%u\n", pt ? pt :net_ntoa(daddr), ntohs(dport));
		if (opt.logf)
			fprintf(opt.logf, "%s.%u\n", pt ? pt :net_ntoa(daddr), ntohs(dport));
	}
	else {
		printf("%s.%u -> ", net_ntoa(saddr), ntohs(sport));
		printf("%s.%u\n",  net_ntoa(daddr), ntohs(dport));
		if (opt.logf) {
			fprintf(opt.logf, "%s.%u -> ", net_ntoa(saddr), ntohs(sport));
			fprintf(opt.logf, "%s.%u\n",  net_ntoa(daddr), ntohs(dport));
		}
	}
	fflush(stdout);
	if (opt.logf)
		fflush(opt.logf);
}


/*
 * Scan TCP packet for password
 */
int
scan_tcp(time_t ts, Wlan_nfo *wi, IPv4_hdr *iph, 
		TCP_hdr *tcph, u_int8_t *payload, size_t paylen)
{
    u_int8_t buf[8192];
	u_int8_t pbuf[2048];
	int decoded = 0;
	u_int len;
	int i;

	/* Scan all protocols */
	for (i=0; tcp_funcs[i].func != NULL; i++) {	

		if ((opt.scan_all == 0) && (tcph->tcp_dprt != ntohs(tcp_funcs[i].port)))
			continue;

		/* Since some routines edit the payload buffer we copy
		 * it to keep the packet in its original state
		 * ugly, but it works for now.
		 * TODO: Rewrite functions not to edit packets */
		memcpy(pbuf, payload, paylen);
		
		if (tcp_funcs[i].func(pbuf, paylen, buf, sizeof(buf)-1) != 0) {
		
			print_banner(ts, wi, tcp_funcs[i].name, iph->ip_sadd, 
				tcph->tcp_sprt, iph->ip_dadd, tcph->tcp_dprt);

			len = strlen(buf);
			if ((len >= 1) && (buf[len-1] == '\n'))
				buf[len-1] = '\0';	
			if ((len >= 2) && (buf[len-2] == '\r'))
				buf[len-2] = '\0';	
			
            printf("%s\n\n", buf);
			if (opt.logf) {
				fprintf(opt.logf, "%s\n\n", buf);
				fflush(opt.logf);
			}
			decoded++;
		}
		else if (opt.traffic) {
			
			if (opt.logf)
				fprintf(opt.logf, "** TCP Traffic ");
			printf("** TCP Traffic ");
			print_banner(ts, wi, tcp_funcs[i].name, iph->ip_sadd,
				tcph->tcp_sprt, iph->ip_dadd, tcph->tcp_dprt);
		}
	}

	opt.decoded += decoded;
	return(decoded);
}

/*
 * Scan UDP packet for password
 */
int
scan_udp(time_t ts, Wlan_nfo *wi, IPv4_hdr *iph,
        UDP_hdr *udph, u_int8_t *payload, size_t paylen)
{
    u_int8_t buf[8192];
    u_int8_t pbuf[2048];
    int decoded = 0;
    u_int len;
    int i;

    /* Scan all protocols */
    for (i=0; udp_funcs[i].func != NULL; i++) {

        if ((opt.scan_all == 0) && (udph->udp_dprt != ntohs(udp_funcs[i].port)))
            continue;

        /* Since some routines edit the payload buffer we copy
         * it to keep the packet in its original state
         * ugly, but it works for now.
         * TODO: Rewrite functions not to edit packets */
        memcpy(pbuf, payload, paylen);

        if (udp_funcs[i].func(pbuf, paylen, buf, sizeof(buf)-1) != 0) {

            print_banner(ts, wi, udp_funcs[i].name, iph->ip_sadd,
                udph->udp_sprt, iph->ip_dadd, udph->udp_dprt);

            len = strlen(buf);
            if ((len >= 1) && (buf[len-1] == '\n'))
                buf[len-1] = '\0';
            if ((len >= 2) && (buf[len-2] == '\r'))
                buf[len-2] = '\0';

            printf("%s\n\n", buf);
            if (opt.logf) {
                fprintf(opt.logf, "%s\n\n", buf);
                fflush(opt.logf);
            }
            decoded++;
        }
        else if (opt.traffic) {

            if (opt.logf)
                fprintf(opt.logf, "** UDP Traffic ");
            printf("** UDP Traffic ");
            print_banner(ts, wi, udp_funcs[i].name, iph->ip_sadd,
                udph->udp_sprt, iph->ip_dadd, udph->udp_dprt);
        }
    }

    opt.decoded += decoded;
    return(decoded);
}

