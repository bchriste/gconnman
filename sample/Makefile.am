noinst_PROGRAMS = sample
sample_SOURCES = sample.c
INCLUDES = @SAMPLE_CFLAGS@
LIBS = @SAMPLE_LIBS@
AM_CFLAGS = -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = $(top_builddir)/gconnman/libgconnman.la 
CLEANFILES = *~

-include $(top_srcdir)/git.mk
