kde_module_LTLIBRARIES = libamarok_xineengine_plugin.la
kde_services_DATA = amarok_xineengine_plugin.desktop

INCLUDES = -I$(top_srcdir)/amarok/src $(CFLAGS_XINE) $(all_includes)

libamarok_xineengine_plugin_la_LIBADD = \
    $(top_builddir)/amarok/src/engine/libengine.la \
    $(top_builddir)/amarok/src/plugin/libplugin.la \
    $(LIB_KDECORE) $(LIB_KDEUI) \
    $(LIBS_XINE)

libamarok_xineengine_plugin_la_SOURCES = xine-scope.c xine-engine.cpp
libamarok_xineengine_plugin_la_LDFLAGS = -module $(KDE_PLUGIN) $(all_libraries)

METASOURCES  = AUTO
