noinst_PROGRAMS = test-service
test_service_SOURCES = test-service.c
INCLUDES = @GCONNMAN_CFLAGS@
LIBS = @GCONNMAN_LIBS@
AM_CFLAGS = -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = $(top_builddir)/gconnman/libgconnman.la
CLEANFILES = *~

-include $(top_srcdir)/git.mk
