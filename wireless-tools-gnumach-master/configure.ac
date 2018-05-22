# Configure script for GNU Mach Wireless Tools
# Copyright (C) 2006  Stefan Siegl <stesie@brokenpipe.de>, Germany
#
dnl Process this file with autoconf to produce a configure script.

#AC_INIT(configure.in)
AC_INIT(wireless-tools-gnumach, 0.1, [stesie@brokenpipe.de])
AM_INIT_AUTOMAKE(wireless-tools-gnumach, 0.1, [stesie@brokenpipe.de])
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC

AC_GNU_SOURCE
AC_HEADER_STDC

AC_CHECK_LIB([m], [log10],, 
             AC_MSG_ERROR([wireless-tools-gnumach require libm]))

AC_OUTPUT([
Makefile
src/Makefile
src/linux/Makefile
])

