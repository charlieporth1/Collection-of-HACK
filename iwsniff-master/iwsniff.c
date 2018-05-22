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
 * $Id: iwsniff.c,v 1.7 2005-02-13 21:08:58 cmn Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include "iwsniff.h"

/* Local variables */
static pcap_t *p;
static time_t start_time;

/* Global variables */
struct options opt;

/* Local routines */
static void usage(char *);
static void print_stats(void);
static  void sig_print_stats(int);


static void
usage(char *pname)
{
    printf("\n");
    printf("\nWireless sniffer\n");
    printf("\nBy PocPon <pocpon@fuzzpoint.com>\n");
	printf("\nUsage: %s [option(s)] [expression]\n\n", pname);
	printf("Options:\n");
	printf("  -b            Do not include BSSID when logging wireless traffic\n");
	printf("  -i interface  Interface to listen on\n"); 
	printf("  -n            Do not attempt to resolve addresses\n");
	printf("  -o logfile    Log output to file\n");
	printf("  -p            Don't put interface in promiscuous mode\n");
	printf("  -r file       Read packets in wiretap format from file\n");
	printf("  -t            Enable traffic detection\n");
	printf("  -v            Verbose\n");
	printf("  -w file       Write successfully decoded packets to file in wiretap format\n");
	printf("  -A            Force each packet to be scanned by every decode routine\n");
	printf("  -D            Run in the backround (daemon)\n");
	printf("\n");
}

/*
 * atexit(3) function
 */
static void
print_stats(void)
{
	sig_print_stats(0);
}

static void
sig_print_stats(int signo)
{
	struct pcap_stat ps;
	memset(&ps, 0x00, sizeof(ps));
	
	vprint(1, "[%s] Capture finished\n", currtime(time(NULL)));

	if (pcap_stats(p, &ps) != 0) {
		errx("Failed to retrieve pcap stats: %s\n", pcap_geterr(p));
		return;
	}
	printf("\nRunning for %u seconds\n", (u_int)(time(NULL) - start_time));
	printf("%u packets received\n", ps.ps_recv);
	printf("%u packets dropped\n", ps.ps_drop);
	printf("%u packets sucessfully decoded\n", opt.decoded);

	if (opt.logf) {
		fprintf(opt.logf, "\nRunning for %u seconds\n", (u_int)(time(NULL) - start_time));
		fprintf(opt.logf, "%u packets received\n", ps.ps_recv);
		fprintf(opt.logf, "%u packets dropped\n", ps.ps_drop);
		fprintf(opt.logf, "%u packets sucessfully decoded\n", opt.decoded);
	}

	if (opt.dumpfile)
		pcap_dump_close(opt.pdt);
	_exit(signo);
}


int
main(int argc, char *argv[])
{
	struct capture *cap;
	char *logfile = NULL;
	char *filter = NULL;
	int i;

	memset(&opt, 0x00, sizeof(struct options));
	opt.resolve = 1;
	opt.promisc = 1;
	opt.daemon = 0;

	while ( (i = getopt(argc, argv, "Avhi:r:npw:tDo:be")) != -1) {
		switch (i) {
			case 'A': opt.scan_all = 1; break;
			case 'b': opt.nobssid = 1; break;
			case 't': opt.traffic = 1; break;
			case 'p': opt.promisc = 0; break;
			case 'n': opt.resolve = 0; break;
			case 'o': logfile = optarg; break;
			case 'v': opt.verbose++; break;
			case 'r': opt.iface = optarg; break;
			case 'i': opt.iface = optarg; break;
			case 'w': 
				opt.dumpfile = optarg; 
				if (is_reg_file(opt.dumpfile)) {
					printf("File '%s' exist, overwrite? [y/n]: ", opt.dumpfile);
					if (getchar() != 'y')
						exit(EXIT_FAILURE);
				}
				break;
			
			case 'D': opt.daemon = 1; break;
			default: 
				usage(argv[0]); 
				exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		filter = str_join(" ", argv);

	/* Open logfile */
	if ( (logfile != NULL) && (opt.logf = fopen(logfile, "a")) == NULL) 
		errx("Failed to open %s: %s\n", logfile, strerror(errno));

	if (opt.daemon == 1) {
		int i = 0;
		
		if ((opt.logf == NULL) && (opt.dumpfile == NULL))
			errx("Either dumpfile (-w) or logfile (-o) must be set when running as daemon\n");
	
		if (daemonize() != 0)
			exit(EXIT_FAILURE);
		printf("[%d]\n", getpid());

		if (opt.logf == NULL) {
			if ( (opt.logf = fopen("/dev/null", "w")) == NULL)
				errx("Error opening /dev/null: %s\n", strerror(errno));
		}

		for (i=0; i<fileno(opt.logf); i++)
			close(i);

		dup2(fileno(opt.logf), fileno(stdout));
		dup2(fileno(opt.logf), fileno(stderr));
		opt.logf = NULL;
	}

	if ( (cap = cap_open(opt.iface, 1)) == NULL)
		errx("Failed to open input, bailing out\n");
	printf("[%s] Opened '%s'\n", currtime(time(NULL)), cap->dev);
	
	if (opt.logf)
		fprintf(opt.logf, "[%s] Opened '%s'\n", currtime(time(NULL)), cap->dev);

	if (filter != NULL) {
		if (cap_setfilter(cap, filter) < 0)
			exit(EXIT_FAILURE);
	
		vprint(1, "Using filter '%s'\n", filter);
		free(filter);
	}
			

	if (opt.verbose || opt.daemon) {
		p = cap->cap_pcapd;
		atexit(print_stats);
		signal(SIGTERM, sig_print_stats);
		signal(SIGINT, sig_print_stats);
		start_time = time(NULL);
	}

	if (opt.dumpfile != NULL) {
		if ( (opt.pdt = pcap_dump_open(cap->cap_pcapd, opt.dumpfile)) == NULL)
			errx("pcap_dump_open: %s\n", pcap_geterr(cap->cap_pcapd));
	}
		
	pcap_loop(cap->cap_pcapd, -1, scanpkt, (u_char *)cap);
	if (opt.dumpfile)
		pcap_dump_close(opt.pdt);
	exit(EXIT_SUCCESS);
}
