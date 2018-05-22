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
 * $Id: scanpkt.c,v 1.9 2007-01-07 00:24:19 cmn Exp $
 */

#include <stdio.h>
#include "iwsniff.h"

/* Global options */
extern struct options opt;

void
scanpkt(u_char *arg,
	const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
	struct capture *cap = (struct capture *)arg;
	int paylen;
	u_int8_t *payload;
	Wlan_nfo wi;
	Wlan_nfo *wp;
	IPv4_hdr *iph;
	TCP_hdr *tcph;
	UDP_hdr *udph;

	/* Check that packet length is OK */
	if ( (pkthdr->len) <= (cap->cap_offst + sizeof(IPv4_hdr))) 
		return;

	/* Create IP header */
	iph = (IPv4_hdr *)(packet + cap->cap_offst);

	/* Only IPv4 allowed */
	if (iph->ip_ver != 4)
		return;

	/* Very verbose, print packet */
	if (opt.verbose > 1) 
		cap_logpkt((char *)iph);

	/* Save wireless info */
	if (cap->cap_datalink == DLT_IEEE802_11)
		wp = set_wlan_nfo(&wi, (u_char *)packet);
	else if (cap->cap_datalink == DLT_PRISM_HEADER) {
		size_t n;

		if (packet[7] == 0x40)
			n = 64;
		else
			n = *(int *)(packet + 4);
		
		if ((n < 8) || (n >= pkthdr->caplen))
			return;
			
		cap->cap_offst = n;	
		wp = set_wlan_nfo(&wi, (u_char *)packet + n);
	}
	else if (cap->cap_datalink == DLT_IEEE802_11_RADIO) {
		size_t n;
		
		 n = *(unsigned short *)(packet + 2);
		 
		 if (n <= 0 || n >= (int)pkthdr->caplen)
			return;

		cap->cap_offst = n;
		wp = set_wlan_nfo(&wi, (u_char *)packet + n);
	}

	/* Scan TCP packet */
	if (iph->ip_prot == IP_PROTO_TCP) {

		/* Create TCP header */
		tcph = (TCP_hdr *)(packet + cap->cap_offst + (iph->ip_hlen)*4);
		paylen = ntohs(iph->ip_tlen) - (4*iph->ip_hlen + 4*tcph->tcp_hlen);
		payload = (u_char *)((u_char *)tcph + 4*(tcph->tcp_hlen));

		if (paylen <= 0)
			return;

		if (scan_tcp(pkthdr->ts.tv_sec, wp, iph, tcph, payload, paylen) &&
				opt.dumpfile) {
#ifdef HAVE_PCAP_DUMP_FLUSH
			pcap_dump((u_char *)opt.pdt, pkthdr, packet);
			pcap_dump_flush(opt.pdt);
#endif
		}
	}
    /* Scan UDP packet */
    else if (iph->ip_prot == IP_PROTO_UDP) {

        /* Create UDP header */
		udph = (UDP_hdr *)(packet + cap->cap_offst + (iph->ip_hlen)*4);
		paylen = ntohs(iph->ip_tlen) - (4*iph->ip_hlen) - sizeof(UDP_hdr);
		payload = (u_char *)((u_char *)udph + sizeof(UDP_hdr));

        if (paylen <= 0)
            return;

        if (scan_udp(pkthdr->ts.tv_sec, wp, iph, udph, payload, paylen) &&
                opt.dumpfile) {
#ifdef HAVE_PCAP_DUMP_FLUSH
            pcap_dump((u_char *)opt.pdt, pkthdr, packet);
            pcap_dump_flush(opt.pdt);
#endif
		}
    }

	if (opt.verbose > 1)
		cap_logpkt((char *)iph);
}

