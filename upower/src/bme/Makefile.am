## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DUP_COMPILATION					\
	-DG_LOG_DOMAIN=\"UPower-BME\"			\
	-I$(top_srcdir)/libupower-glib				\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)					\
	$(GLIB_CFLAGS)

if BACKEND_TYPE_BME
noinst_LTLIBRARIES = libupshared.la
endif

libupshared_la_SOURCES =					\
	up-backend.c						\
	up-native.c						\
	bmeipc.c						\
	$(BUILT_SOURCES)

libupshared_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local :
	rm -f *~

-include $(top_srcdir)/git.mk
