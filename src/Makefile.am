## Process this file with automake to produce Makefile.in

bin_PROGRAMS = lxhotkey

lxhotkey_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_PLUGINS_DIR=\""$(libdir)/lxhotkey"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS)

lxhotkey_SOURCES = lxhotkey.c

lxhotkey_LDADD = $(PACKAGE_LIBS)

lxhotkey_includedir = $(includedir)/lxhotkey
lxhotkey_include_HEADERS = lxhotkey.h
