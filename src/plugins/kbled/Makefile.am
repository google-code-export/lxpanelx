INCLUDES = \
    -I. \
    -I$(top_srcdir)/src \
    $(PACKAGE_CFLAGS) \
    $(G_CAST_CHECKS)

module_LTLIBRARIES = kbled.la

moduledir = $(pkglibdir)/plugins

kbled_la_SOURCES = kbled.c

kbled_la_LIBADD = $(PACKAGE_LIBS)

kbled_la_LDFLAGS = \
    -module \
    @LXPANELX_MODULE@
