## Process this file with automake to produce Makefile.in

bin_PROGRAMS = lxkeys

lxkeys_CPPFLAGS = \
        -I$(top_srcdir) \
        -DPACKAGE_PLUGINS_DIR=\""$(libdir)/lxkeys"\" \
        -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        $(PACKAGE_CFLAGS)

lxkeys_SOURCES = lxkeys.c

lxkeys_LDADD = $(PACKAGE_LIBS)
