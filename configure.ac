AC_PREREQ(2.61)

# package version number (not shared library version)
m4_define(gconnman_version_major, 0)
m4_define(gconnman_version_minor, 5)
m4_define(gconnman_version_micro, 0)

m4_define([gconnman_version],
          [gconnman_version_major.gconnman_version_minor.gconnman_version_micro])
m4_define([gconnman_api_version],
          [gconnman_version_major.gconnman_version_minor])

AC_INIT(gconnman, gconnman_version)
AM_INIT_AUTOMAKE(gconnman, gconnman_version)
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_HEADERS(config.h)

AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GCONNMAN, glib-2.0 gthread-2.0 dbus-glib-1 gobject-2.0)

AC_SUBST(GCONNMAN_CFLAGS)
AC_SUBST(GCONNMAN_LIBS)

AC_ARG_ENABLE([sample],
      AS_HELP_STRING([--enable-sample], [Build sample GTK application]),
      [ if test "$enableval" = no; then
	  sample=no
	else
	  sample=yes
	fi], [sample=no])

if test "x$sample" = "xyes"; then
   PKG_CHECK_MODULES(SAMPLE, glib-2.0 gtk+-2.0 gobject-2.0 gthread-2.0)
   SAMPLE_CFLAGS="$GCONNMAN_CFLAGS $SAMPLE_CFLAGS"
   SAMPLE_LIBS="$GCONNMAN_LIBS $SAMPLE_LIBS"
   AC_SUBST(SAMPLE_CFLAGS)
   AC_SUBST(SAMPLE_LIBS)
fi

#GTK_DOC_CHECK([1.9])

AM_CONDITIONAL([SAMPLE], [test "x$sample" = "xyes"])
#AM_COND_IF isn't deployed to the masses yet (it lands in aclocal-1.10.3)...
# so we can't conditionally generate the sample/Makefile.
# Make it for everyone instead:
#AM_COND_IF([SAMPLE], [AC_CONFIG_FILES([sample/Makefile])])

SHAVE_INIT([build/autotools], [enable])

AC_OUTPUT([Makefile
          build/Makefile
          build/autotools/Makefile
          build/autotools/shave
          build/autotools/shave-libtool
          gconnman/Makefile
          gconnman/gconnman.pc
	  sample/Makefile
          tests/Makefile
	  ])
#	  doc/Makefile
#	  doc/reference/Makefile

echo ""
echo "   gconnman $VERSION"
echo "   ====================="
echo "   Documentation: ${enable_gtk_doc}"
echo "   GTK Sample   : ${sample}"
echo ""
echo "   To build the project, run \"make\""
echo "   To install run \"make install\""
echo ""

