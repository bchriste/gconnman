EXTRA_DIST = autogen.sh
#DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
#SUBDIRS = gconnman doc
SUBDIRS = gconnman
if SAMPLE
SUBDIRS += sample
endif
CLEANFILES = *~

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
	$(srcdir)/missing
