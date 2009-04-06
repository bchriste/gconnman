AC_PREREQ(2.61)

# package version number (not shared library version)
# odd micro numbers indicate in-progress development
# even micro numbers indicate released versions
m4_define(gconnman_version_major, 0)
m4_define(gconnman_version_minor, 1)
m4_define(gconnman_version_micro, 4)

m4_define([gconnman_version],
          [gconnman_version_major.gconnman_version_minor.gconnman_version_micro])
m4_define([gconnman_api_version],
          [gconnman_version_major.gconnman_version_minor])

AC_INIT(gconnman, gconnman_version)
AM_INIT_AUTOMAKE(gconnman, gconnman_version)
AC_CONFIG_HEADERS(config.h)

# Check for programs
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

# enable pkg-config
PKG_PROG_PKG_CONFIG

# Check for function availability
# AC_CHECK_FUNCS([memset])

# all libraries listed are associated with "gconnman" tag
PKG_CHECK_MODULES(GCONNMAN, glib-2.0
                  dbus-glib-1
		  gobject-2.0
		  glib-2.0)

AC_SUBST(GCONNMAN_CFLAGS)
AC_SUBST(GCONNMAN_LIBS)

GTK_DOC_CHECK([1.9])

# Output files
AC_OUTPUT([Makefile
gconnman/Makefile
gconnman/gconnman.pc
doc/Makefile
doc/reference/Makefile
])
#doc/reference/version.xml

echo ""
echo "   gconnman $VERSION"
echo "   ====================="
#echo "   Documentation: ${enable_gtk_doc}"
echo ""
echo "   To build the project, run \"make\""
echo ""

