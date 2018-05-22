/*
 * NTE, a program to enable Telnet access to a Netgear router.
 *
 * Adapted from: https://github.com/insanid/NetgearTelnetEnable
 *
 * Notes from that source:
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * This program is a re-implementation of the telnet console enabler utility
 * for use with Netgear wireless routers.
 *
 * The original Netgear Windows binary version of this tool is available here:
 * http://www.netgear.co.kr/Support/Product/FileInfo.asp?IDXNo=155
 *
 * Per DMCA 17 U.S.C. §1201(f)(1)-(2), the original Netgear executable was
 * reverse engineered to enable interoperability with other operating systems
 * not supported by the original windows-only tool (MacOS, Linux, etc).
 *
 *       Netgear Router - Console Telnet Enable Utility
 *       Release 0.1 : 25th June 2006
 *       Copyright (C) 2006, yoshac @ member.fsf.org
 *       Release 0.2 : 20th August 2012
 *       dj bug fix on OS X
 *       Release 0.3 : 8th October 2012
 *       keithr-git bug fix to send entire packet in one write() call,
 *         strcpy->strncpy, clean up some whitespace
 *
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License along
 *       with this program; if not, write to the Free Software Foundation, Inc.,
 *       51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The RSA MD5 and Blowfish implementations are provided under LGPL from
 * http://www.opentom.org/Mkttimage
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

/* Modifications by Steven M. Schweda, 2017-08-01:
 *
 * In general, the code which constructs the message to be sent to the
 * router is largely unchanged.  Most other code has been replaced.
 * 
 * - Changed the program name to NTE.
 * - Adapted to VMS.  (See also: descrip.mms, vms.c.)
 * - Adapted to Windows.
 * - Added a simple command-line parser, using named parameters instead
 *   of positional parameters.
 * - Added some debug diagnostic messages.
 * - Changed to allow lower-case characters and colons in a MAC address.
 * - Reduced the potential for buffer overflows from user-specified
 *   parameters.
 * - Added type casts to clear compiler complaints about signed v.
 *   unsigned char pointers.
 * - Added a check for an "ACK" response from the router to determine
 *   success.
 */

/*--------------------------------------------------------------------*/
/*    Program identification. */

#define PROGRAM_DATE            "2017-08-01"
#define PROGRAM_VERSION_MAJ     0
#define PROGRAM_VERSION_MIN     5

/*--------------------------------------------------------------------*/
/*    Symbolic constants. */

/* Debug category flags. */

#define DBG_ARG        0x00000001       /* Arguments. */
#define DBG_DNS        0x00000002       /* DNS, name resolution. */
#define DBG_SIO        0x00000004       /* Socket I/O. */
#define DBG_WIN        0x00000008       /* Windows-specific. */

/*--------------------------------------------------------------------*/
/*    Command-line options. */

char *opts[] =
 {      "debug",        "debug=",       "mac=",         "name=",
        "password=",    "user="
 };

#define OPT_DEBUG               0
#define OPT_DEBUG_EQ            1
#define OPT_MAC_EQ              2
#define OPT_NAME_EQ             3
#define OPT_PASSWORD_EQ         4
#define OPT_USER_EQ             5

/*--------------------------------------------------------------------*/
/*    Header files, derived macros. */

#ifdef _WIN32
# define _CRT_SECURE_NO_WARNINGS
# include <io.h>
# include <Winsock2.h>
# include <Ws2tcpip.h>
# define ssize_t SSIZE_T
# define BAD_SOCKET( s) ((s) == INVALID_SOCKET)
# define CLOSE_SOCKET closesocket
# define IN_PORT_T unsigned short
# define STRNCASECMP _strnicmp
# define WRITE _write
#else /* def _WIN32 */
#include <libgen.h>
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# define BAD_SOCKET( s) ((s) < 0)
# define CLOSE_SOCKET close
# define IN_PORT_T in_port_t
# define INVALID_SOCKET (-1)
# define SOCKET int
# define STRNCASECMP strncasecmp
# define WRITE write
#endif /* def _WIN32 [else] */

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "md5.h"
#include "blowfish.h"

# ifndef RECVFROM_6
#  define RECVFROM_6 unsigned int       /* Type for arg 6 of recvfrom(). */
# endif /* ndef RECVFROM_6 */           /* ("int", "socklen_t", ...?) */

/*--------------------------------------------------------------------*/
/*    Miscellaneous macros. */

#define SOCKET_TIMEOUT     500000       /* Microseconds.  (0.5s) */

#define NTE_MIN( a, b) (((a) <= (b)) ? (a) : (b))

#define PORT_TELNET ((unsigned short)23)

/*--------------------------------------------------------------------*/
/*    Global storage. */

static int debug;                               /* Debug flag(s). */

static char out_buf[0x640];

static BLOWFISH_CTX ctx;

static struct PAYLOAD
{
  char signature[ 16];
  char mac[ 16];
  char username[ 16];
  char password[ 33];
  char reserved[ 47];
} payload;

static char *progname;

/*--------------------------------------------------------------------*/
/*    Functions. */

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* usage(): Explain program usage. */

/* Note: The debug=mask argument can be any constant which is accepted
 * by strtol().  This includes a decimal value, a hexadecimal value with
 * "0x" prefix, an octal value with "0" prefix, and so on.
 */

void usage( void)
{
  fprintf( stderr, "\n");
  fprintf( stderr, "%s %d.%d (%s)\n",
   progname, PROGRAM_VERSION_MAJ, PROGRAM_VERSION_MIN, PROGRAM_DATE);
  fprintf( stderr, "\n");
#ifdef __VMS
  fprintf( stderr, "Usage (DCL: %s == \"$ dev:[dir]%s.exe\"):\n",
   progname, progname);
#else
  fprintf( stderr, "Usage:\n");
#endif
  fprintf( stderr,
 "   %s mac=mac_addr name=dns_name password=pass user=user_name\n",
   progname);
  fprintf( stderr,
 "        [debug[=mask]]\n");
  fprintf( stderr, "\n");
  fprintf( stderr,
   "Keywords may be abbreviated and in any case (e.g.: M=mac_addr).\n");
  fprintf( stderr,
   "MAC address (mac_addr) may include colons, and digits a-f in any case.\n");
  fprintf( stderr,
   "DNS name (dns_name) may be a name or an IP address.\n");
  fprintf( stderr,
   "See source for debug details.  \"debug\" with no mask enables all.\n");
  fprintf( stderr, "\n");
  fprintf( stderr,
   "Example:\n");
  fprintf( stderr,
   "   %s m=50:6b:03:e9:ad:86 n=192.168.0.1 u=admin p=password\n",
   progname);
  fprintf( stderr, "\n");
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* keyword_match(): Match abbreviated keyword against keyword array. */

int keyword_match( char *arg, int kw_cnt, char **kw_array)
{
  int cmp_len;
  int kw_ndx;
  int match = -1;

  cmp_len = strlen( arg);
  for (kw_ndx = 0; kw_ndx < kw_cnt; kw_ndx++)
  {
    if (STRNCASECMP( arg, kw_array[ kw_ndx], cmp_len) == 0)
    {
      if (match < 0)
      {
        match = kw_ndx;         /* Record first match. */
      }
      else
      {
        match = -2;             /* Record multiple match, */
        break;                  /* and quit early. */
      }
    }
  }

  return match;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* show_errno(): Display error code(s), message. */

int show_errno( void)
{
  int sts;

#ifdef _WIN32
  sts = fprintf( stderr, "%s: errno = %d, WSAGLA() = %d.\n",
   progname, errno, WSAGetLastError());
#else /* def _WIN32 */
# ifdef VMS
  sts = fprintf( stderr, "%s: errno = %d, vaxc$errno = %08x.\n",
   progname, errno, vaxc$errno);
# endif /* def VMS [else] */
#endif /* def _WIN32 [else]*/

  if (sts >= 0)
  {
    fprintf( stderr, "%s: %s\n", progname, strerror( errno));
  }

  return sts;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* dns_resolve(): Resolve DNS name to IPv4 address using getaddrinfo(). */

int dns_resolve( char *name, struct in_addr *ip_addr)
{
  int sts;
  struct addrinfo *ai_pp;       /* addrinfo() result (linked list). */
  struct addrinfo ai_h;         /* addrinfo() hints. */

  /* Fill getaddrinfo() hints structure. */
  memset( &ai_h, 0, sizeof( ai_h));
  ai_h.ai_family = AF_INET;
  ai_h.ai_protocol = IPPROTO_UDP;

  sts = getaddrinfo( name,              /* Node name. */
                     NULL,              /* Service name. */
                     &ai_h,             /* Hints. */
                     &ai_pp);           /* Results (linked list). */

  if (sts != 0)
  {
    if ((debug& DBG_DNS) != 0)
    {
      fprintf( stderr, " getaddrinfo() failed.\n");
    }
  }
  else
  { /* Use the first result in the ai_pp linked list. */
    if ((debug& DBG_DNS) != 0)
    {
      fprintf( stderr, " getaddrinfo():\n");
      fprintf( stderr,
       " ai_family = %d, ai_socktype = %d, ai_protocol = %d\n",
       ai_pp->ai_family, ai_pp->ai_socktype, ai_pp->ai_protocol);

      fprintf( stderr,
       " ai_addrlen = %d, ai_addr->sa_family = %d.\n",
       ai_pp->ai_addrlen, ai_pp->ai_addr->sa_family);

      fprintf( stderr,
       " ai_flags = 0x%08x.\n", ai_pp->ai_flags);

      if (ai_pp->ai_addr->sa_family == AF_INET)
      {
        fprintf( stderr, " ai_addr->sa_data[ 2: 5] = %u.%u.%u.%u\n",
         (unsigned char)ai_pp->ai_addr->sa_data[ 2],
         (unsigned char)ai_pp->ai_addr->sa_data[ 3],
         (unsigned char)ai_pp->ai_addr->sa_data[ 4],
         (unsigned char)ai_pp->ai_addr->sa_data[ 5]);
      }
    }

#ifndef NO_AF_CHECK     /* Disable the address-family check everywhere. */
    {
# if defined( VMS) && defined( VAX) && !defined( GETADDRINFO_OK)
      /* Work around apparent bug in getaddrinfo() in TCPIP V5.3 - ECO 4
       * on VMS V7.3 (typical hobbyist kit?).  sa_family seems to be
       * misplaced, misaligned, corrupted.  Spurious sa_len?
       */
      int sa_fam;
#  define SA_FAMILY sa_fam

      if (ai_pp->ai_addr->sa_family != AF_INET)
      {
        if ((debug& DBG_DNS) != 0)
        {
          fprintf( stderr,
           " Unexpected address family (not %d, V-V work-around): %d.\n",
           AF_INET, ai_pp->ai_addr->sa_family);
        }
        SA_FAMILY = ai_pp->ai_addr->sa_family/ 256;     /* Try offset. */
      }
# else /* defined( VMS) && defined( VAX) && !defined( GETADDRINFO_OK) */
#  define SA_FAMILY ai_pp->ai_addr->sa_family
# endif /* defined( VMS) && defined( VAX) && !defined( GETADDRINFO_OK) [else] */

      if (SA_FAMILY != AF_INET)
      { /* Complain about (original) bad value. */
        fprintf( stderr, "%s: Unexpected address family (not %d): %d.\n",
         progname, AF_INET, ai_pp->ai_addr->sa_family);
        sts = -1;
      }
    }
#endif /* ndef NO_AF_CHECK */

    if ((sts == 0) && (ip_addr != NULL))
    { /* IPv4 address (net order). */
      memcpy( ip_addr, &ai_pp->ai_addr->sa_data[ 2], sizeof( *ip_addr));
    }
  }

  return sts;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* GetOutputLength() */

int GetOutputLength(unsigned long lInputLong)
{
  unsigned long lVal = lInputLong % 8;

  if (lVal != 0)
    return lInputLong+8-lVal;
  else
    return lInputLong;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* EncodeString() */

int EncodeString(BLOWFISH_CTX *ctx,char *pInput,char *pOutput, int lSize)
{
  int SameDest = 0;
  int lCount = 0;
  int lOutSize;
  int i;

  lOutSize = GetOutputLength(lSize);
  while (lCount < lOutSize)
    {
      char *pi = pInput;
      char *po = pOutput;
      for (i = 0; i < 8; i++)
        *po++ = *pi++;
      Blowfish_Encrypt(ctx,(uint32_t *)pOutput,(uint32_t *)(pOutput+4));
      pInput += 8;
      pOutput += 8;
      lCount += 8;
    }

  return lCount;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* fill_payload() */

int fill_payload( char *mac, char *user, char *pass)
{
  MD5_CTX MD;
  char MD5_key[0x10];
  char secret_key[0x400]="AMBIT_TELNET_ENABLE+";
  int encoded_len;

  memset(&payload, 0, sizeof(payload));

  if (mac != NULL)
    strncpy( payload.mac, mac, sizeof( payload.mac));

  if (user != NULL)
    strncpy( payload.username, user, sizeof( payload.username));

  if (pass != NULL)
    strncpy( payload.password, pass, sizeof( payload.password));

  MD5Init(&MD);
  MD5Update( &MD, (unsigned char *)payload.mac, 0x70);
  MD5Final( (unsigned char *)MD5_key, &MD);
  strncpy(payload.signature, MD5_key, sizeof(payload.signature));
  // NOTE: so why concatenate outside of the .signature boundary again
  //       using strcat? deleting this line would keep the payload the same and not
  //       cause some funky abort() or segmentation fault on newer gcc's
  // dj: this was attempting to put back the first byte of the MAC address
  // dj: which was getting stomped by the strcpy of the MD5_key above
  // dj: a better fix is to use strncpy to avoid the stomping in the 1st place
  //  strcat(payload.signature, mac);

  if (pass != NULL)
    strncat(secret_key,pass,sizeof(secret_key) - strlen(secret_key) - 1);

  Blowfish_Init( &ctx, (unsigned char *)secret_key, strlen(secret_key));

  encoded_len = EncodeString(&ctx,(char*)&payload,(char*)&out_buf,0x80);

  return encoded_len;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* main() */

int main( int argc, char **argv)
{
  int ack = 0;
  char ack_msg[] = { 'A', 'C', 'K' };
  int arg;
  char *arg_mac = NULL;
  char *arg_name = NULL;
  char *arg_pass = NULL;
  char *arg_user = NULL;
  char mac_str[ sizeof( payload.mac)+ 1];
  ssize_t bc = -1;
  size_t cmp_len;
  ssize_t ibc;
  int opts_ndx;
  size_t out_buf_len;
  unsigned char msg_inp[ 1024]; /* Receive message buffer. */
  int sts = EXIT_SUCCESS;
  fd_set fds_rec;
  struct timeval timeout_rec;
  SOCKET sock = INVALID_SOCKET;
  struct in_addr ip_addr;
  RECVFROM_6 sock_addr_len;
  struct sockaddr_in sock_addr;

#ifdef _WIN32
  char sock_opt_rec = 1;
#else /* def _WIN32 */
  unsigned int sock_opt_rec = 1;
#endif /* def _WIN32 [else] */

#ifdef __VMS
  extern char *vms_basename( char *file_spec);
# define BASENAME vms_basename
#else /* def __VMS */
# ifdef WIN32
  extern char *win_basename( char *file_spec);
#  define BASENAME win_basename
# else /* def WIN32 */
#  define BASENAME basename
# endif /* def WIN32 [else] */
#endif /* def __VMS [else] */

  debug = 0;
  progname = BASENAME( argv[ 0]);

#ifdef _WIN32

  /* Windows socket library, start-up, ... */

# pragma comment( lib, "ws2_32.lib")    /* Arrange WinSock link library. */

  if (sts == EXIT_SUCCESS)
  {
    WORD ws_ver_req = MAKEWORD( 2, 2);  /* Request WinSock version 2.2. */
    WSADATA wsa_data;

    sts = WSAStartup( ws_ver_req, &wsa_data);
    if (sts != 0)
    {
      fprintf( stderr, "%s: Windows socket start-up failed.  sts = %d.\n",
       progname, sts);
      show_errno();
      sts = EXIT_FAILURE;
    }
    else if ((debug& DBG_WIN) != 0)
    {
      /* LOBYTE = MSB, HIBYTE = LSB. */
      fprintf( stderr, " wVersion = %d.%d, wHighVersion = %d.%d.\n",
       LOBYTE( wsa_data.wVersion), HIBYTE( wsa_data.wVersion),
       LOBYTE( wsa_data.wHighVersion), HIBYTE( wsa_data.wHighVersion));
    }
  }
#endif /* def _WIN32 */

  /* Check command-line arguments. */

  if (sts == EXIT_SUCCESS)
  {
    char *eqa_p;
    char *eqo_p;
    int opts_cnt = sizeof( opts)/ sizeof( *opts);
    int match_opt;

    for (arg = 1; argv[ arg] != NULL; arg++)
    { /* Parse/skip option arguments. */
      match_opt = -1;
      eqa_p = strchr( argv[ arg], '=');         /* Find "=" in arg. */
      for (opts_ndx = 0; opts_ndx < opts_cnt; opts_ndx++)   /* Try all opts. */
      {
        eqo_p = strchr( opts[ opts_ndx], '=');  /* Find "=" in opt. */
        if (! ((eqa_p == NULL) ^ (eqo_p == NULL)))
        { /* "=" in both arg and opt, or neither. */
          if (eqa_p == NULL)
          { /* No "=" in argument/option.  Compare whole strings. */
            cmp_len = NTE_MIN( strlen( argv[ arg]), strlen( opts[ opts_ndx]));
          }
          else
          { /* "=".  Compare strings before "=". */
            cmp_len = (eqa_p- argv[ arg]);
          }
          if (STRNCASECMP( argv[ arg], opts[ opts_ndx], cmp_len) == 0)
          {
            if (match_opt < 0)
            {
              match_opt = opts_ndx;     /* Record first match. */
            }
            else
            {
              match_opt = -2;           /* Record multiple match, */
              break;                    /* and quit early. */
            }
          } /* if arg-opt match */
        } /* if "=" match */
      } /* for opts_ndx */

      /* All options checked.  Act accordingly. */
      if (match_opt < -1)
      { /* Multiple match. */
        fprintf( stderr, "%s: Ambiguous option: %s\n",
         progname, argv[ arg]);
        errno = EINVAL;
        sts = EXIT_FAILURE;
        break; /* while */
      }
      else if (match_opt < 0)           /* Not a known option.  Arg? */
      {
        break; /* while */
      }
      else                              /* Recognized option. */
      {
        if (match_opt == OPT_DEBUG)             /* "debug". */
        {
          match_opt = -1;                       /* Consumed. */
          debug = 0xffffffff;
          fprintf( stderr, " debug:  0x%08x .\n", debug);
        }
        else if (match_opt == OPT_DEBUG_EQ)     /* "debug=". */
        {
          match_opt = -1;                       /* Consumed. */
          debug = strtol( (argv[ arg]+ cmp_len+ 1), NULL, 0);
          fprintf( stderr, " debug = 0x%08x .\n", debug);
        }
        else if (match_opt == OPT_MAC_EQ)       /* "mac=". */
        {
          match_opt = -1;                       /* Consumed. */
          arg_mac = argv[ arg]+ cmp_len+ 1;
        }
        else if (match_opt == OPT_NAME_EQ)      /* "name=". */
        {
          match_opt = -1;                       /* Consumed. */
          arg_name  = argv[ arg]+ cmp_len+ 1;
        }
        else if (match_opt == OPT_PASSWORD_EQ)  /* "password=". */
        {
          match_opt = -1;                       /* Consumed. */
          arg_pass = argv[ arg]+ cmp_len+ 1;
        }
        else if (match_opt == OPT_USER_EQ)      /* "user=". */
        {
          match_opt = -1;                       /* Consumed. */
          arg_user = argv[ arg]+ cmp_len+ 1;
        }

        if (match_opt >= 0)             /* Unexpected option. */
        { /* Match, but no handler. */
          fprintf( stderr,
           "%s: Unexpected option (bug), code = %d.\n",
           progname, match_opt);
          errno = EINVAL;
          sts = EXIT_FAILURE;
          break; /* while */
        }
      }
    } /* for */
  }

  if ((arg_mac == NULL) ||
   (arg_name == NULL) || (arg_pass == NULL) || (arg_user == NULL))
  {
    usage();
    sts = EXIT_FAILURE;
  }

  if (sts == EXIT_SUCCESS)
  {
    int dst_i;
    char *src_p;

    /* Process MAC string: Convert to upper case, delete ":". */

    for (dst_i = 0, src_p = arg_mac; *src_p; src_p++)
    {
      if (*src_p != ':')
      {
        mac_str[ dst_i++] = toupper( *src_p);
        if (dst_i > sizeof( payload.mac))
          break;
      }
    }
    mac_str[ dst_i] = '\0';

    if (*src_p != '\0')
    {
      fprintf( stderr,
       "%s: MAC address (processed) too long (> %ld characters): >%s<.\n",
        progname, sizeof( payload.mac), mac_str);
      sts = EXIT_FAILURE;
    }

    if (strlen( arg_user) > sizeof( payload.username))
    {
      fprintf( stderr,
       "%s: User name too long (> %ld characters): >%s<.\n",
        progname, sizeof( payload.username), arg_user);
      sts = EXIT_FAILURE;
    }
    if (strlen( arg_pass) > sizeof( payload.password))
    {
      fprintf( stderr,
       "%s: Password too long (> %ld characters): >%s<.\n",
        progname, sizeof( payload.password), arg_pass);
      sts = EXIT_FAILURE;
    }

    if ((debug& DBG_ARG) != 0)
    {
      fprintf( stderr, " MAC0: >%s<.\n", arg_mac);
      fprintf( stderr, " MAC1: >%s<.\n", mac_str);
      fprintf( stderr, " user: >%s<.\n", arg_user);
      fprintf( stderr, " pass: >%s<.\n", arg_pass);
    }
  }

  if (sts == EXIT_SUCCESS)
  {
    out_buf_len = fill_payload( mac_str, arg_user, arg_pass);

    sts = dns_resolve( arg_name, &ip_addr);     /* Try to resolve the name. */

    if ((debug& DBG_DNS) != 0)
    {
      fprintf( stderr, " dns_resolve() sts = %d.\n", sts);
    }

    if (sts != 0)
    {
      fprintf( stderr,
       "%s: DNS name/address resolution failed.\n", progname);
      show_errno();
      sts = EXIT_FAILURE;
    }
  }

  if (sts == EXIT_SUCCESS)
  {
    sock_addr.sin_family = AF_INET;               /* Address family. */
    sock_addr.sin_addr.s_addr = ip_addr.s_addr;   /* (IP) address. */
    sock_addr.sin_port = htons( PORT_TELNET);     /* Port. */

    sock = socket( AF_INET, SOCK_DGRAM, 0);
    if (BAD_SOCKET( sock))
    {
      fprintf( stderr, "%s: socket() failed.\n", progname);
      show_errno();
      sts = EXIT_FAILURE;
    }
  }

  if (sts == EXIT_SUCCESS)
  {
    if (setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &sock_opt_rec,
     sizeof( sock_opt_rec)) < 0)
    {
      fprintf( stderr, "%s: setsockopt() failed.\n", progname);
      show_errno();
      sts = EXIT_FAILURE;
    }
  }

  if (sts == EXIT_SUCCESS)
  {
    bc = sendto( sock,                          /* Socket. */
                 out_buf,                       /* Message. */
                 out_buf_len,                   /* Message length. */
                 0,                             /* Flags (not MSG_OOB). */
                 (struct sockaddr *)
                  &sock_addr,                   /* Socket address. */
                 sizeof( sock_addr));           /* Socket address length. */

    if (bc < (ssize_t)out_buf_len)
    {
      fprintf( stderr, "%s: sendto() failed.\n", progname);
      show_errno();
      sts = EXIT_FAILURE;
    }
  }

  if (sts == EXIT_SUCCESS)
  {
    /* Fill file-descriptor flags and time-out value for select(). */
    memset( &fds_rec, 0, sizeof( fds_rec));
    FD_SET( sock, &fds_rec);
    timeout_rec.tv_sec  = SOCKET_TIMEOUT/ 1000000;      /* Seconds. */
    timeout_rec.tv_usec = SOCKET_TIMEOUT% 1000000;      /* Microseconds. */

    if ((debug& DBG_SIO) != 0)
    {
      fprintf( stderr, " sock = %d, FD_SETSIZE = %d,\n",
       sock, FD_SETSIZE);

      fprintf( stderr, " FD_ISSET( sock, &fds_rec) = %d.\n",
       FD_ISSET( sock, &fds_rec));
    }
  
  }

  /* Read responses until recvfrom()/select() times out. */
  while (bc >= 0)
  {
    if ((debug& DBG_SIO) != 0)
    {
      fprintf( stderr, " pre-select(1).  sock = %d.\n", sock);
    }

    sts = select( FD_SETSIZE, &fds_rec, NULL, NULL, &timeout_rec);

    if (sts <= 0)
    {
      if (sts < 0)
      {
        fprintf( stderr, "%s: select(1) failed.\n", progname);
        show_errno();
      }
      else if ((debug& DBG_SIO) != 0)
      {
        fprintf( stderr, " select(1) sts = %d.\n", sts);
      }
      break;
    }
    else
    {
      if ((debug& DBG_SIO) != 0)
      {
        fprintf( stderr, " pre-recvfrom().  sock = %d.\n",
         sock);
      }

      /* Receive a response. */
      sock_addr_len = sizeof( sock_addr);       /* Socket addr len. */
      bc = recvfrom( sock,
                     msg_inp,
                     sizeof( msg_inp),
                     0,                         /* Flags (not OOB or PEEK). */
                     (struct sockaddr *)
                      &sock_addr,               /* Socket address. */
                     &sock_addr_len);           /* Socket address length. */

      if (bc < 0)
      {
        fprintf( stderr, "%s: recvfrom() failed.\n", progname);
        show_errno();
        sts = EXIT_FAILURE;
      }
      else
      {
        if ((debug& DBG_SIO) != 0)
        {
          fprintf( stderr, " recvfrom() bc = %ld.\n",
           (long)bc);

          if (sock_addr.sin_family == AF_INET)
          {
            unsigned int ia4;

            fprintf( stderr, " s_addr: %08x .\n",
             sock_addr.sin_addr.s_addr);

            ia4 = ntohl( sock_addr.sin_addr.s_addr);
            fprintf( stderr, " recvfrom() addr: %u.%u.%u.%u\n",
             ((ia4/ 0x100/ 0x100/ 0x100)& 0xff),
             ((ia4/ 0x100/ 0x100)& 0xff),
             ((ia4/ 0x100)& 0xff),
             (ia4& 0xff));
          }
        }

        if ((debug& DBG_SIO) != 0)
        {
          for (ibc = 0; ibc < bc; ibc++)
          {
            fprintf( stderr, " %02x", msg_inp[ ibc]);
          }
          fprintf( stderr, "  %.*s\n", (int)bc, msg_inp);
        }

        if (bc == sizeof( ack_msg))
        {
          if (memcmp( msg_inp, ack_msg, bc) == 0)
          {
            ack = 1;
            fprintf( stderr,
             "%s: Received ACK message.  Telnet access should be enabled.\n",
             progname);
          }
        }
      }
    }
  }

  if (! BAD_SOCKET( sock))
  {
    CLOSE_SOCKET( sock);
  }

  if ((sts == EXIT_SUCCESS) && (ack == 0))
  {
    fprintf( stderr, "%s: FAIL.  No ACK message received.\n",
     progname);
    sts = EXIT_FAILURE;
  }

  return sts;
}
