## Process this file with automake to produce Makefile.in

## common flags for all plugins
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)

AM_LDFLAGS = \
	-module -avoid-version -shared -export-dynamic -no-undefined \
	-rpath $(libdir)/lxhotkey $(PACKAGE_LIBS)

## modules list
pkglibdir = $(libdir)/lxhotkey
pkglib_LTLIBRARIES = ob.la

## Openbox module
ob_la_SOURCES = openbox.c
ob_la_LIBADD = -lfm-extra

install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/*.la

PLUGINS_INSTALLED = $(pkglib_LTLIBRARIES:.la=.so)
uninstall-hook:
	cd $(DESTDIR)$(pkglibdir) && rm -f $(PLUGINS_INSTALLED) || true
