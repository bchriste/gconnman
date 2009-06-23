
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gconnman.pc

# connman-marshal defines the marshalling code used for the signals
# sourced by ConnMan
MARSHALFILES = connman-marshal.c connman-marshal.h
BUILT_SOURCES = $(MARSHALFILES)

library_includedir=$(includedir)/gconnman
library_include_HEADERS = gconnman.h \
	cm-manager.h cm-device.h cm-network.h cm-service.h cm-connection.h

#Tell library where data directory is (/usr/share/gconnman)
AM_CFLAGS = -Wall -DPKGDATADIR="\"$(pkgdatadir)\""

INCLUDES = @GCONNMAN_CFLAGS@

lib_LTLIBRARIES = libgconnman.la

libgconnman_la_SOURCES = gconnman-internal.h \
	cm-manager.c cm-device.c cm-network.c cm-service.c cm-connection.c \
	$(MARSHALFILES)

libgconnman_la_LIBADD = @GCONNMAN_LIBS@
libgconnman_la_LDFLAGS= -version-info 0:1:0 -no-undefined

connman-marshal.c: connman-marshal.list Makefile
	glib-genmarshal --prefix=connman_marshal --body $< > $@

connman-marshal.h: connman-marshal.list Makefile
	glib-genmarshal --prefix=connman_marshal --header $< > $@

DISTCLEANFILES = $(MARSHALFILES)
EXTRA_DIST = connman-marshal.list

CLEANFILES = *~ gconnman.pc


-include $(top_srcdir)/git.mk
