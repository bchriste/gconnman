AC_PREREQ(2.61)

# package version number (not shared library version)
# odd micro numbers indicate in-progress development
# even micro numbers indicate released versions
m4_define(gconnman_version_major, 0)
m4_define(gconnman_version_minor, 1)
m4_define(gconnman_version_micro, 5)

m4_define([gconnman_version],
          [gconnman_version_major.gconnman_version_minor.gconnman_version_micro])
m4_define([gconnman_api_version],
          [gconnman_version_major.gconnman_version_minor])

AC_INIT(gconnman, gconnman_version)
AM_INIT_AUTOMAKE(gconnman, gconnman_version)
AC_CONFIG_HEADERS(config.h)

AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GCONNMAN, glib-2.0 dbus-glib-1 gobject-2.0)

AC_SUBST(GCONNMAN_CFLAGS)
AC_SUBST(GCONNMAN_LIBS)

AC_ARG_ENABLE([sample],
      AS_HELP_STRING([--enable-sample], 
                     [Build sample GTK application]),
		     sample=false, sample=true)

if test "x$sample" = "xtrue"; then
   PKG_CHECK_MODULES(SAMPLE, glib-2.0 gtk+-2.0 gobject-2.0 gthread-2.0)
   SAMPLE_CFLAGS="$GCONNMAN_CFLAGS $SAMPLE_CFLAGS"
   SAMPLE_LIBS="$GCONNMAN_LIBS $SAMPLE_LIBS"
   AC_SUBST(SAMPLE_CFLAGS)
   AC_SUBST(SAMPLE_LIBS)
fi

GTK_DOC_CHECK([1.9])

AM_CONDITIONAL([SAMPLE], [test "x$sample" = xtrue])
#AM_COND_IF isn't deployed to the masses yet (it lands in aclocal-1.10.3)... 
# so we can't conditionally generate the sample/Makefile.  
# Make it for everyone instead:
#AM_COND_IF([SAMPLE], [AC_CONFIG_FILES([sample/Makefile])])

AC_OUTPUT([Makefile
          gconnman/Makefile
          gconnman/gconnman.pc
	  doc/Makefile
	  doc/reference/Makefile
	  sample/Makefile
	  ])

echo ""
echo "   gconnman $VERSION"
echo "   ====================="
echo "   Documentation: ${enable_gtk_doc}"
echo "   GTK Sample   : ${sample}"
echo ""
echo "   To build the project, run \"make\""
echo "   To install run \"make install\""
echo ""

