dnl Copyright (C) 2007 Wimba, Inc.

AC_PREREQ(2.59)

dnl package version
m4_define(VIDCAP_VERSION, [0.2.1])

AC_INIT([libvidcap], VIDCAP_VERSION, [jpgrayson@gmail.com])
AC_CONFIG_SRCDIR([include/vidcap/vidcap.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall])
AM_MAINTAINER_MODE
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl libtool library version
VC_LT_CURRENT=1
VC_LT_REVISION=0
VC_LT_AGE=0

AC_SUBST(VC_LT_CURRENT)
AC_SUBST(VC_LT_REVISION)
AC_SUBST(VC_LT_AGE)

AC_CANONICAL_HOST

AC_GNU_SOURCE

AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_C_CONST
AC_PROG_GCC_TRADITIONAL
AC_PROG_CXX
AC_PROG_INSTALL
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

ACX_PTHREAD

AC_CHECK_PROG(have_pkg_config, pkg-config, yes, no)

if test x"$have_pkg_config" = "xno"; then
   AC_MSG_ERROR(pkg-config is required to install this program)
fi

PKG_PROG_PKG_CONFIG

AC_CHECK_FUNCS(nanosleep gettimeofday snprintf)

AC_CHECK_HEADER(linux/videodev.h,
    have_v4l=yes, have_v4l=no)

AC_CHECK_HEADER(QuickTime/QuickTime.h,
    have_quicktime=yes, have_quicktime=no)

AC_CHECK_HEADER(DShow.h,
    have_directshow=yes, have_directshow=no)

if test "x$have_v4l" = "xyes"; then
  AC_DEFINE(HAVE_V4L, [], [video for linux headers present])
fi

if test "x$have_quicktime" = "xyes"; then
  AC_DEFINE(HAVE_QUICKTIME, [], [quicktime headers present])
fi

if test "x$have_directshow" = "xyes"; then
  AC_DEFINE(HAVE_DIRECTSHOW, [], [directshow headers present])
fi

AM_CONDITIONAL(HAVE_V4L, test "x$have_v4l" = "xyes")
AM_CONDITIONAL(HAVE_QUICKTIME, test "x$have_quicktime" = "xyes")
AM_CONDITIONAL(HAVE_DIRECTSHOW, test "x$have_directshow" = "xyes")

dnl TODO: how do we make the various quicktime frameworks fit in?
AC_SUBST(PKG_REQUIRES)

AC_CONFIG_FILES([
  Makefile
  include/Makefile
  include/vidcap/Makefile
  examples/Makefile
  vidcap.pc
  src/Makefile
])

AC_OUTPUT
