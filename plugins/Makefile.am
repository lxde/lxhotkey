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
if WITH_GTK
pkglib_LTLIBRARIES += gtk.la
endif

## Openbox module
ob_la_SOURCES = openbox.c
ob_la_LIBADD = -lfm-extra

# GTK module
gtk_la_SOURCES = gtk.c
gtk_la_CFLAGS = $(GTK_CFLAGS)
gtk_la_LIBADD = $(GTK_LIBS)


# .desktop files
desktopdir=$(datadir)/applications
desktop_in_files =
if WITH_GTK
desktop_in_files += lxhotkey-gtk.desktop.in
endif
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST= \
	lxhotkey-gtk.desktop.in lxhotkey-gtk.desktop

install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/*.la

PLUGINS_INSTALLED = $(pkglib_LTLIBRARIES:.la=.so)
uninstall-hook:
	cd $(DESTDIR)$(pkglibdir) && rm -f $(PLUGINS_INSTALLED) || true
