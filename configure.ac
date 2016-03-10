AC_PREREQ(2.53)
AC_INIT(lxkeys, 0.1.0, http://lxde.org/)
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects no-dist-gzip dist-xz])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Support silent build rules. Disable by either passing --disable-silent-rules
# to configure or passing V=1 to make
AM_SILENT_RULES([yes])

# Force to dynamic
AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
IT_PROG_INTLTOOL([0.40.0], [no-xml])
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# International support
GETTEXT_PACKAGE=lxkeys
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

AM_GLIB_GNU_GETTEXT

# Handle --enable-debug option
AC_ARG_ENABLE(debug,
    [AC_HELP_STRING([--enable-debug],
        [enable debug support @<:@default=no@:>@])],
    [enable_debug="${enableval}"],
    [enable_debug=no]
)
if test "$enable_debug" = "yes"; then
    # turn on debug and disable optimization
    CPPFLAGS="$CPPFLAGS -DG_ENABLE_DEBUG -O0 -g"
    case "$CC" in
    gcc*)
        CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror=implicit-function-declaration"
        ;;
    *)
        ;;
    esac
    dnl Be more strict on portability
    #CPPFLAGS="$CPPFLAGS -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=700"
else
    # turn off glib debug checks
    CPPFLAGS="$CPPFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
fi

dnl Test for required libraries
pkg_modules="libfm >= 1.2.0 x11"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

dnl Test for libunistring for correct UTF-8 printf
AC_CHECK_LIB(unistring, ulc_fprintf)

dnl Fix invalid sysconfdir when --prefix=/usr
if test `eval "echo $sysconfdir"` = /usr/etc
then
    sysconfdir=/etc
fi

dnl Finish all
AC_CONFIG_FILES([
    Makefile
    lxkeys.pc
    src/Makefile
    plugins/Makefile
    po/Makefile.in
])
AC_OUTPUT
