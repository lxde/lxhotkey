AC_PREREQ(2.53)
AC_INIT(lxhotkey, 0.1.1, http://lxde.org/)
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
GETTEXT_PACKAGE=lxhotkey
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

AM_GLIB_GNU_GETTEXT

# Generate po/LINGUAS for available PO files
rm -f $srcdir/po/LINGUAS
for po_file in `ls $srcdir/po/*.po | sort`;
do
    lang=`echo "$po_file" | sed "s|.*/po/\(.*\)\.po|\1|g"`
    echo $lang >> $srcdir/po/LINGUAS
done

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

dnl GTK+ stuff
AC_ARG_WITH(gtk,
    [AC_HELP_STRING([--with-gtk=VER],
        [build 'gtk' GUI, choose GTK+ version for build])
AC_HELP_STRING([--without-gtk],
        [disable 'gtk' GUI build @<:@default@:>@])],
    [ac_with_gtk="${withval}"],
    [ac_with_gtk=no]
)

if test x"$ac_with_gtk" != xno; then
  case "$ac_with_gtk" in
    3|3.*)
      pkg_modules="gtk+-3.0"
      ;;
    *)
      pkg_modules="gtk+-2.0 >= 2.18.0"
      ;;
  esac
  PKG_CHECK_MODULES(GTK, [$pkg_modules], [with_gtk=yes], [with_gtk=no])
fi
AM_CONDITIONAL(WITH_GTK, [test x$with_gtk = xyes])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl Test for libunistring for correct UTF-8 printf
AC_CHECK_LIB(unistring, ulc_fprintf)

dnl Supress extra linking
AC_MSG_CHECKING([whether $LD accepts --as-needed])
case `$LD --as-needed -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
    LDFLAGS="$LDFLAGS -Wl,--as-needed"
    AC_MSG_RESULT([yes])
    ;;
*)
    AC_MSG_RESULT([no])
    ;;
esac

dnl Fix invalid sysconfdir when --prefix=/usr
if test `eval "echo $sysconfdir"` = /usr/etc
then
    sysconfdir=/etc
fi

dnl Finish all
AC_CONFIG_FILES([
    Makefile
    lxhotkey.pc
    src/Makefile
    plugins/Makefile
    po/Makefile.in
    man/lxhotkey.1
])
AC_OUTPUT
