#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#
CC          = gcc
CFLAGS      = -Wall -s #-DHAVE_PCAP_DUMP_FLUSH
LIBS        = -lpcap -lcrypto
OBJS        = iwsniff.o utils.o strlcat.o capture.o net.o scanpkt.o base64.o \
              hex.o scan.o asn1.o rpc.o mount.o strlcpy.o decode.o buf.o daemon.o

DECODE_OBJS	= decode_aim.o decode_citrix.o decode_cvs.o decode_ftp.o \
              decode_hex.o decode_http.o decode_icq.o decode_imap.o \
			  decode_irc.o decode_ldap.o decode_mmxp.o decode_mountd.o \
			  decode_napster.o decode_nntp.o decode_oracle.o decode_ospf.o \
			  decode_pcanywhere.o decode_pop.o \
			  decode_postgresql.o decode_pptp.o decode_rip.o decode_rlogin.o \
			  decode_smb.o decode_smtp.o decode_sniffer.o decode_snmp.o \
			  decode_socks.o decode_tds.o decode_telnet.o decode_vrrp.o \
			  decode_x11.o decode_yp.o
PROG        = iwsniff

# Install path
# Root dir must end with '/' to avoid trouble ..
ROOT_DIR         = /usr/local/iwsniff/
BIN_DIR          = ${ROOT_DIR}/bin
MAN_DIR          = ${ROOT_DIR}usr/man

#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#
all: rpcstuff ${OBJS} ${DECODE_OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS} ${DECODE_OBJS} ${LIBS}

static: rpcstuff ${OBJS} ${DECODE_OBJS}
	${CC} ${CFLAGS} -DSTATIC_BIN -static -o ${PROG} ${OBJS} ${DECODE_OBJS} ${LIBS}

clean:
	rm -f *.o ${PROG} mount.h mount.c

rpcstuff:
	rpcgen -h mount.x -o mount.h
	rpcgen -c mount.x -o mount.c
	${CC} ${CFLAGS} -c mount.c

new: clean all

install:
	@strip ${PROG}
	@mkdir -p ${BIN_DIR}
	@strip ${PROG}
	@chmod 0755 ${BIN_DIR}
	@chown root:0 ${PROG}
	@chmod 0555 ${PROG}
	cp -pi ${PROG} ${BIN_DIR}/${PROG}
#    @mkdir -p ${MAN_DIR}/man1
#    @chown root:0 ${MAN1_FILES}
#    @chmod 0444 ${MAN1_FILES}
#    cp -pi ${MAN1_FILES} ${MAN_DIR}/man1/

uninstall:
	rm -f ${BIN_DIR}/${PROG}
	rmdir ${BIN_DIR}
	rmdir ${ROOT_DIR}
#    PWD=`pwd`; cd ${MAN_DIR}/man1/; rm -f ${MAN1_FILES}; cd ${PWD}

