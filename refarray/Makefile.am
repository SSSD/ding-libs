TRACE_LEVEL=@TRACE_VAR@

topdir=$(srcdir)/..

AM_CFLAGS =

if HAVE_GCC
    AM_CFLAGS += \
        -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual \
        -Wcast-align -Wwrite-strings
endif

AM_CPPFLAGS = $(TRACE_LEVEL)

ACLOCAL_AMFLAGS = -I m4

# Set up the pkg-config file
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ref_array.pc

dist_noinst_DATA = m4 contrib/libref_array.spec.in

dist_include_HEADERS = ref_array.h

# Build library
lib_LTLIBRARIES = libref_array.la
libref_array_la_SOURCES = \
    ref_array.c \
    trace.h
libref_array_la_LDFLAGS = \
    -version-info 1:0:0

# Build unit test
check_PROGRAMS = ref_array_ut
ref_array_ut_SOURCES = ref_array_ut.c
ref_array_ut_LDADD = libref_array.la

if HAVE_DOXYGEN
docs:
# A temp way of building docs until we hook into the package build
	rm -fR doc
	mkdir -p doc
	$(DOXYGEN) ref_array.cfg.doxy
else
docs:
	@echo "Doxygen not installed, cannot generate documentation"
	@exit 1
endif


TESTS = ref_array_ut

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
	cp $(builddir)/contrib/libref_array.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" -ba SPECS/libref_array.spec

# make srpms will use the old digest algorithm to be compatible
# with RHEL5
srpm: dist-gzip rpmroot
	cp $(builddir)/contrib/libref_array.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" \
	         --define _source_filedigest_algorithm=1 \
	         -bs SPECS/libref_array.spec
