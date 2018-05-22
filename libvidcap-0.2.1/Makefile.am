SUBDIRS = include src examples

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vidcap.pc

EXTRA_DIST = \
	autogen.sh \
	contrib

dist-hook:
	find $(distdir) -name ".svn" -type d -print0 | xargs -0 rm -rf
