#DEBUG_FLAGS=@DEBUG_VAR@
TRACE_LEVEL=@TRACE_VAR@

COLLECTION_CFLAGS=@COLLECTION_CFLAGS@
COLLECTION_LIBS=@COLLECTION_LIBS@

PATH_UTILS_CFLAGS=@PATH_UTILS_CFLAGS@
PATH_UTILS_LIBS=@PATH_UTILS_LIBS@

REFARRAY_CFLAGS=@REFARRAY_CFLAGS@
REFARRAY_LIBS=@REFARRAY_LIBS@

builddir ?= .

topdir=$(srcdir)/..
topbuilddir=$(builddir)/..

AM_CFLAGS =
if HAVE_GCC
    AM_CFLAGS += \
        -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual \
        -Wcast-align -Wwrite-strings
endif

AM_CPPFLAGS = \
    $(COLLECTION_CFLAGS) \
    $(PATH_UTILS_CFLAGS) \
    $(REFARRAY_CFLAGS) \
    $(TRACE_LEVEL)

ACLOCAL_AMFLAGS = -I m4

# Set up the pkg-config file
pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = ini_config.pc

dist_noinst_DATA = \
    ini.conf \
    ini.d/real.conf \
    ini.d/test.conf \
    m4 \
    contrib/libini_config.spec.in

dist_include_HEADERS = \
    ini_config.h

# Build library
lib_LTLIBRARIES = libini_config.la
libini_config_la_SOURCES = \
    ini_config.c \
    ini_config.h \
    ini_get_value.c \
    ini_get_array.c \
    ini_list.c \
    ini_print.c \
    ini_parse.c \
    ini_parse.h \
    ini_metadata.c \
    ini_metadata.h \
    ini_defines.h \
    ini_comment.c \
    ini_comment.h \
    trace.h


libini_config_la_LIBADD = \
    $(COLLECTION_LIBS) \
    $(PATH_UTILS_LIBS) \
    $(REFARRAY_LIBS)
libini_config_la_LDFLAGS = \
    -version-info 2:0:0

# Build unit test
check_PROGRAMS = ini_config_ut ini_comment_ut
ini_config_ut_SOURCES = ini_config_ut.c
ini_config_ut_LDADD = \
    libini_config.la \
    $(LIBCOLLECTION_LIBS)
ini_comment_ut_SOURCES = ini_comment_ut.c
ini_comment_ut_LDADD = libini_config.la

if HAVE_DOXYGEN
docs:
# A temp way of building docs until we hook into the package build
	rm -fR doc
	mkdir -p doc
	$(DOXYGEN) ini_config.cfg.doxy
else
docs:
	@echo "Doxygen not installed, cannot generate documentation"
	@exit 1
endif

TESTS = ini_config_ut ini_comment_ut

tests: all $(check_PROGRAMS)

clean-local:
	rm -Rf doc

builddir ?= .
RPMBUILD ?= $(PWD)/rpmbuild

rpmroot:
	mkdir -p $(RPMBUILD)/BUILD
	mkdir -p $(RPMBUILD)/RPMS
	mkdir -p $(RPMBUILD)/SOURCES
	mkdir -p $(RPMBUILD)/SPECS
	mkdir -p $(RPMBUILD)/SRPMS

rpms: dist-gzip rpmroot
	cp $(builddir)/contrib/libini_config.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" -ba SPECS/libini_config.spec

# make srpms will use the old digest algorithm to be compatible
# with RHEL5
srpm: dist-gzip rpmroot
	cp $(builddir)/contrib/libini_config.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" \
	         --define _source_filedigest_algorithm=1 \
	         -bs SPECS/libini_config.spec
