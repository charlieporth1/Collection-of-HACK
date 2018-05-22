/*
 *  File: capture.h
 *
 *  Copyright (c) 2002 Claes M. Nyberg <md0claes@mdstud.chalmers.se>
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
 * $Id: capture.c,v 1.10 2004-12-04 12:04:43 cmn Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pcap.h>
#include "capture.h"
#include "iwsniff.h"

extern struct options opt;

/*
 * Opens a device to capture packets from (NULL for lookup).
 * Returns a NULL pointer on error and a pointer
 * to a struct capture on success.
 * Arguments:
 *  dev     - Device to open (NULL for lookup)
 *  promisc - Should be one for open in promisc mode, 0 otherwise
 */
struct capture *
cap_open(u_char *dev, int promisc)
{
    u_char ebuf[PCAP_ERRBUF_SIZE];   /* Pcap error string */
    struct capture cap;
	struct capture *pt;

	/* Open file */
	if (is_reg_file(dev)) {
		
		if (file_size(dev) == 0) {
			err("Target file '%s' is empty\n", dev);
			return(NULL);
		}
		
		if ( (cap.cap_pcapd = pcap_open_offline(dev, ebuf)) == NULL) {
			err("%s\n", err);
			return(NULL);
		}
	}
	/* Open device */
	else {

		/* Let pcap pick an interface to listen on */
		if (dev == NULL) {
			if ( (dev = pcap_lookupdev(ebuf)) == NULL) {
				err("%s\n", err);
				return(NULL);
			}
		}

    	/* Init pcap */
    	if (pcap_lookupnet(dev, &cap.cap_net, 
				&cap.cap_mask, ebuf) != 0) 
        	warn("%s\n", ebuf);

    	/* Open the interface */
    	if ( (cap.cap_pcapd = pcap_open_live(dev, 
        	    CAP_SNAPLEN, promisc, CAP_TIMEOUT, ebuf)) == NULL) {
        	err("%s\n", ebuf);
        	return(NULL);
    	}
	}

    /* Set linklayer offset 
	 * Offsets gatheret from various places (Ethereal, ipfm, ..) */
    switch( (cap.cap_datalink = pcap_datalink(cap.cap_pcapd))) {

#ifdef DLT_EN10MB
        case DLT_EN10MB:
            cap.cap_offst = 14;
            vprint(1, "Resolved datalink to Ethernet\n");
            break;
#endif
#ifdef DLT_ARCNET
        case DLT_ARCNET:
            cap.cap_offst = 6;
            vprint(1, "Resolved datalink to ARCNET\n");
            break;
#endif
#ifdef DLT_PPP_ETHER
        case DLT_PPP_ETHER:
            cap.cap_offst = 8;
            vprint(1, "Resolved datalink to PPPoE\n");
            break;
#endif
#ifdef DLT_NULL
        case DLT_NULL:
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to BSD "
                "loopback encapsulation\n");
            break;
#endif
#ifdef DLT_LOOP
        case DLT_LOOP:
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to OpenBSD "
                "loopback encapsulation\n");
            break;
#endif
#ifdef DLT_PPP
        case DLT_PPP:
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to PPP\n");
            break;
#endif
#ifdef DLT_C_HDLC
        case DLT_C_HDLC:        /* BSD/OS Cisco HDLC */
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to Cisco PPP with HDLC framing\n");
            break;
#endif
#ifdef DLT_PPP_SERIAL
        case DLT_PPP_SERIAL:    /* NetBSD sync/async serial PPP */
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to PPP in HDLC-like framing\n");
            break;
#endif
#ifdef DLT_RAW
        case DLT_RAW:
            cap.cap_offst = 0;
            vprint(1, "Resolved datalink to raw IP\n");
            break;
#endif
#ifdef DLT_SLIP
        case DLT_SLIP:
            cap.cap_offst = 16;
            vprint(1, "Resolved datalink to SLIP\n");
            break;
#endif
#ifdef DLT_SLIP_BSDOS
        case DLT_SLIP_BSDOS:
            cap.cap_offst = 24;
            vprint(1, "Resolved datalink to DLT_SLIP_BSDOS\n");
            break;
#endif
#ifdef DLT_PPP_BSDOS
        case DLT_PPP_BSDOS:
            cap.cap_offst = 24;
            vprint(1, "Resolved datalink to DLT_PPP_BSDOS\n");
            break;
#endif
#ifdef DLT_ATM_RFC1483
        case DLT_ATM_RFC1483:
            cap.cap_offst = 8;
            vprint(1, "Resolved datalink to RFC 1483 "
                "LLC/SNAP-encapsulated ATM\n");
            break;
#endif
#ifdef DLT_IEEE802
        case DLT_IEEE802:
            cap.cap_offst = 22;
            vprint(1, "Resolved datalink to IEEE 802.5 Token Ring\n");
            break;
#endif
#ifdef DLT_IEEE802_11
        case DLT_IEEE802_11:
            cap.cap_offst = 32;
            vprint(1, "Resolved datalink to IEEE 802.11 wireless LAN\n");
            break;
#endif
#ifdef DLT_ATM_CLIP
        /* Linux ATM defines this */
        case DLT_ATM_CLIP:
            cap.cap_offst = 8;
            vprint(1, "Resolved datalink to DLT_ATM_CLIP\n");
            break;
#endif
#ifdef DLT_PRISM_HEADER
        case DLT_PRISM_HEADER:
            cap.cap_offst = 144+32;
            vprint(1, "Resolved datalink to Prism monitor mode\n");
            break;
#endif
#ifdef DLT_LINUX_SLL
        /* fake header for Linux cooked socket */
        case DLT_LINUX_SLL:
            cap.cap_offst = 16;
            vprint(1, "Resolved datalink to Linux "
                "\"cooked\" capture encapsulation\n");
            break;
#endif
#ifdef DLT_LTALK
        case DLT_LTALK:
            cap.cap_offst = 0;
            vprint(1, "Resolved datalink to Apple LocalTalk\n");
            break;
#endif
#ifdef DLT_PFLOG
        case DLT_PFLOG:
            cap.cap_offst = 50;
            vprint(1, "Resolved datalink to OpenBSD pflog\n");
            break;
#endif
#ifdef DLT_SUNATM
        case DLT_SUNATM:
            cap.cap_offst = 4;
            vprint(1, "Resolved datalink to SunATM device\n");
            break;
#endif
#if 0
        /* Unknown offsets */
        case DLT_IP_OVER_FC: /* RFC  2625  IP-over-Fibre Channel */
        case DLT_FDDI: /* FDDI */
        case DLT_FRELAY: /* Frame Relay */
        case DLT_IEEE802_11_RADIO:
        case DLT_ARCNET_LINUX:
        case DLT_LINUX_IRDA:
#endif
        default:
            err("Unknown datalink type (%d) received for iface %s\n", 
				pcap_datalink(cap.cap_pcapd), dev);
            return(NULL);
    }
	cap.dev = dev;	
	
	pt = zmemx(sizeof(struct capture));
    memcpy(pt, &cap, sizeof(struct capture));
	return(pt);
}

/*
 * Set capture filter.
 * Returns -1 on error and 1 on success.
 */
int
cap_setfilter(struct capture *cap, u_char *filter)
{
    struct bpf_program fp; /* Holds compiled program */

    /* Compile filter string into a program and optimize the code */
    if (pcap_compile(cap->cap_pcapd, &fp, filter, 1, cap->cap_net) == -1) {
        err("Filter: %s\n", pcap_geterr(cap->cap_pcapd));
        return(-1);
    }

    /* Set filter */
    if (pcap_setfilter(cap->cap_pcapd, &fp) == -1) {
        err("Failed to set pcap filter\n");
        return(-1);
    }

	pcap_freecode(&fp);

    return(1);
}


/*
 * Print TCP/UDP/ICMP packet
 */

/* We use this for payloads */
#define BYTETABLE \
    ".................................!\"#$%&'()*+,-./0123456789:;<=>?@"\
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~..."\
    ".................................................................."\
    "............................................................"

int
cap_logpkt(char *pkt)
{
    IPv4_hdr *iph = (IPv4_hdr *)pkt;
    TCP_hdr *tcph = NULL;
    UDP_hdr *udph = NULL;
    ICMP_hdr *ich = NULL;
    u_char *payload = NULL;
    u_char *pt;
    int paylen = 0;


	if (!opt.verbose)
		return(0);

    if (pkt == NULL) {
        warn("cap_logpkt(): Received NULL pointer as packet\n");
        return(-1);
    }

    if (iph->ip_ver != 4) {
        err("Bad IP version (%x), only version 4 supported\n",
            iph->ip_ver);
        return(-1);
    }

    if (iph->ip_prot == IP_PROTO_TCP) {
        tcph = (TCP_hdr *)(pkt + sizeof(IPv4_hdr));

        if ( (paylen = ntohs(iph->ip_tlen) -
                (4*iph->ip_hlen + 4*tcph->tcp_hlen)) > 0)
            payload = (u_char *)((u_char *)tcph + 4*(tcph->tcp_hlen));

        printf("(TCP) ");
    }

    else if (iph->ip_prot == IP_PROTO_UDP) {
        udph = (UDP_hdr *)(pkt + sizeof(IPv4_hdr));

        if ( (paylen = ntohs(iph->ip_tlen) -
                (4*iph->ip_hlen + sizeof(UDP_hdr))) > 0)
            payload = (u_char *)((u_char *)udph + sizeof(UDP_hdr));

        printf("(UDP) ");
    }

    else if (iph->ip_prot == IP_PROTO_ICMP) {
        ich = (ICMP_hdr *)(pkt + sizeof(IPv4_hdr));

        if ( (paylen = ntohs(iph->ip_tlen) -
                (4*iph->ip_hlen + sizeof(ICMP_hdr))) > 0)
            payload = (u_char *)((u_char *)ich + sizeof(ICMP_hdr));

        printf("(ICMP) ");
    }

    else {
        warn("Unrecognized protocol: 0x%02x\n", iph->ip_prot);
        return(-1);
    }

    printf("%s", net_ntoa(iph->ip_sadd));

    if (tcph != NULL)
        printf(":%u", ntohs(tcph->tcp_sprt));

    else if (udph != NULL)
        printf(":%u", ntohs(udph->udp_sprt));

    if (opt.resolve == 1) {
        if ( (pt = net_hostname2(iph->ip_sadd)) != NULL)
            printf(" (%s)", net_hostname2(iph->ip_sadd));
    }

    printf(" -> %s", net_ntoa(iph->ip_dadd));

    if (tcph != NULL)
        printf(":%u", ntohs(tcph->tcp_dprt));
    else if (udph != NULL)
        printf(":%u", ntohs(udph->udp_dprt));

    if (opt.resolve == 1) {
        if ( (pt = net_hostname2(iph->ip_dadd)) != NULL)
            printf(" (%s)", pt);
    }

    printf(" tos=0x%02x id=0x%04x ttl=%u ",
        iph->ip_tos, ntohs(iph->ip_id), iph->ip_ttl);

    printf("hlen=0x%x chksum=0x%02x ",
        iph->ip_hlen, ntohs(iph->ip_sum));

    if (tcph != NULL) {
        printf("seq=0x%08x ack=0x%08x flags=%x win=0x%04x ",
            ntohl(tcph->tcp_seq), ntohl(tcph->tcp_ack),
            tcph->tcp_flgs, ntohs(tcph->tcp_win));
    }

    if (ich != NULL) {
        printf("type=%d code=%d icmp_id=%d icmp_seq=%d ",
            ich->icmp_type, ich->icmp_code, ich->icmp_u32.icmp_echo.id,
            ich->icmp_u32.icmp_echo.seq);
    }

    if ((opt.verbose > 3) && (payload != NULL)) {
        int i = 0;

        printf("    ");
        printf("(%u bytes payload) ", paylen);
        for(; i<paylen; i++)
            printf("%c", BYTETABLE[payload[i]]);
    }
    printf("\n");

    return(0);
}

/*
 * Set wireless information
 * BSSID, src and dst MAC
 */
Wlan_nfo *
set_wlan_nfo(Wlan_nfo *wi, u_char *packet)
{
    u_int16_t fc;
	u_char *sa = NULL;
	u_char *da = NULL;
	u_char *bssid = NULL;
	struct mgmt_header_t *hp;
	

    fc = *((u_int16_t *)packet);
	memset(wi, 0x00, sizeof(Wlan_nfo));

    switch(FC_TYPE(fc)) {

        /* Manegement (Not yet used) */
        case T_MGMT:
			hp = (struct mgmt_header_t *)packet;		
			memcpy(wi->bssid, hp->bssid, sizeof(wi->bssid));
			memcpy(wi->sa, hp->sa, sizeof(wi->sa));
			memcpy(wi->da, hp->da, sizeof(wi->da));
            break;

        /* Control */
        case T_CTRL:
			/* Not yet used */
            break;

        /* Data */
        case T_DATA:
			#define ADDR1  (packet + 4)
			#define ADDR2  (packet + 10)
			#define ADDR3  (packet + 16)
			#define ADDR4  (packet + 24)

		    if (!FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
				bssid = ADDR3;
				sa = ADDR2;
				da = ADDR1;
		    } 
			else if (!FC_TO_DS(fc) && FC_FROM_DS(fc)) {
				bssid = ADDR2;
				sa = ADDR3;
				da = ADDR1;
		    } 
			else if (FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
				bssid = ADDR1;
				sa = ADDR2;
				da = ADDR3;
		    } 
			else if (FC_TO_DS(fc) && FC_FROM_DS(fc)) {
				sa = ADDR4;
				da = ADDR3;
				/* RA = ADDR1 */
				/* TA = ADDR2 */
		    }

			#undef ADDR1
			#undef ADDR2
			#undef ADDR3
			#undef ADDR4

			if (bssid != NULL)
				memcpy(wi->bssid, bssid, sizeof(wi->bssid));
			if (sa != NULL)
				memcpy(wi->sa, sa, sizeof(wi->sa));
			if (da != NULL)
				memcpy(wi->da, da, sizeof(wi->da));
            break;

        default:
            warn("Unknown IEEE802.11 frame type (%d)\n", FC_TYPE(fc));
            return(NULL);
    }

    return(wi);
}

