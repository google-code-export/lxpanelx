indicator_la_CFLAGS = \
	-I. \
	-I$(top_srcdir)/src \
	-DINDICATOR_DIR=\""$(INDICATORDIR)"\" \
	-DINDICATOR_ICONS_DIR=\""$(INDICATORICONSDIR)"\" \
	$(PACKAGE_CFLAGS) \
	$(APPLET_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = indicator.la

moduledir = $(pkglibdir)/plugins

indicator_la_SOURCES = \
	indicator.c

indicator_la_LIBADD = \
	$(PACKAGE_LIBS) \
	$(APPLET_LIBS)

indicator_la_LDFLAGS = \
	-module \
	@LXPANELX_MODULE@
