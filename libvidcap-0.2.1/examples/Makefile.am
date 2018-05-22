AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

noinst_PROGRAMS = simplegrab

simplegrab_LDADD = $(PTHREAD_LIBS) $(top_builddir)/src/libvidcap.la
simplegrab_CFLAGS = $(PTHREAD_CFLAGS)
AM_LDFLAGS = $(PTHREAD_CFLAGS)

simplegrab_SOURCES = simplegrab.c

if HAVE_QUICKTIME
AM_LDFLAGS +=				\
	-framework Carbon		\
	-framework QuartzCore		\
	-framework QuickTime
endif

EXTRA_DIST = vidcapTester
