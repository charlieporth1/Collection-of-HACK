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
 * $Id: scan.h,v 1.5 2009-03-18 08:52:19 cmn Exp $
 */

#ifndef _SCAN_H
#define _SCAN_H

#include <pcap.h>
#include <ctype.h>
#include "capture.h"

#define IGNORE_LEADING_SPACE(pt)	{ for(;isspace((int)*pt);pt++); if (*pt == '\0') return(0); }

/* scanpkt.c */
extern void scanpkt(u_char *, const struct pcap_pkthdr *, const u_char *);

/* base64.c */
extern char *base64_encode(char *, char *, size_t);
extern char *base64_decode(char *, char *, size_t);

/* scan.c */
extern int scan_tcp(time_t, Wlan_nfo *, IPv4_hdr *, TCP_hdr *, u_int8_t *, size_t);
extern int scan_udp(time_t, Wlan_nfo *, IPv4_hdr *, UDP_hdr *, u_int8_t *, size_t);

/* decode_*.c */
extern int decode_http(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_ftp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_aim(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_citrix(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_cvs(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_ftp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_hex(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_icq(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_imap(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_irc(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_ldap(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_mmxp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_mountd(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_napster(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_nntp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_oracle(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_ospf(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_pcanywhere(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_pop(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_portmap(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_postgresql(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_pptp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_rip(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_rlogin(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_smb(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_smtp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_sniffer(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_snmp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_socks(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_tds(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_telnet(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_vrrp(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_x11(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_ypserv(u_int8_t *, size_t, u_int8_t *, size_t);
extern int decode_yppasswd(u_int8_t *, size_t, u_int8_t *, size_t);

#endif /* _SCAN_H */
