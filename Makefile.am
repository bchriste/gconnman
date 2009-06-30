EXTRA_DIST = autogen.sh
#DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
#SUBDIRS = gconnman doc
SUBDIRS = gconnman tests build
if SAMPLE
SUBDIRS += sample
endif
CLEANFILES = *~

ACLOCAL_AMFLAGS = -I build/autotools

-include $(top_srcdir)/git.mk

MAINTAINERCLEANFILES = \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	`find "$(srcdir)" -type f -name Makefile.in -print`

