#DEBUG_FLAGS=@DEBUG_VAR@
TRACE_LEVEL=@TRACE_VAR@

DOXYGEN = @DOXYGEN@

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
dist_pkgconfig_DATA = collection.pc

dist_include_HEADERS = \
    collection.h \
    collection_tools.h \
    collection_queue.h \
    collection_stack.h

dist_noinst_DATA = m4 contrib/libcollection.spec.in

# Build library
lib_LTLIBRARIES = libcollection.la
libcollection_la_SOURCES = \
    collection.c \
    collection_tools.c \
    collection_cnv.c \
    collection_queue.c \
    collection_stack.c \
    collection_cmp.c \
    collection_iter.c \
    collection_priv.h \
    trace.h
libcollection_la_LDFLAGS = \
    -version-info 2:0:0

# Build unit test
check_PROGRAMS = collection_ut collection_stack_ut collection_queue_ut
collection_ut_SOURCES = collection_ut.c
collection_ut_LDADD = libcollection.la
collection_stack_ut_SOURCES = collection_stack_ut.c
collection_stack_ut_LDADD = libcollection.la
collection_queue_ut_SOURCES = collection_queue_ut.c
collection_queue_ut_LDADD = libcollection.la

if HAVE_DOXYGEN
docs:
# A temp way of building docs until we hook into the package build
	rm -fR doc
	mkdir -p doc
	$(DOXYGEN) collection.cfg.doxy
else
docs:
	@echo "Doxygen not installed, cannot generate documentation"
	@exit 1
endif


TESTS = collection_ut collection_stack_ut collection_queue_ut

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
	cp $(builddir)/contrib/libcollection.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" -ba SPECS/libcollection.spec

# make srpms will use the old digest algorithm to be compatible
# with RHEL5
srpm: dist-gzip rpmroot
	cp $(builddir)/contrib/libcollection.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" \
	         --define _source_filedigest_algorithm=1 \
	         -bs SPECS/libcollection.spec
