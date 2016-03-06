## Process this file with automake to produce Makefile.in

## common flags for all plugins
AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/src \
        $(PACKAGE_CFLAGS)

AM_LDFLAGS = \
        -module -avoid-version -shared -export-dynamic \
        -no-undefined $(PACKAGE_LIBS)

## modules list
pkglibdir = $(libdir)/lxkeys
pkglib_LTLIBRARIES = ob.la

## Openbox module
ob_la_SOURCES = openbox.c
