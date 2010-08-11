AM_CFLAGS =
if HAVE_GCC
    AM_CFLAGS += \
        -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual \
        -Wcast-align -Wwrite-strings
endif

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = dhash.pc

dist_noinst_DATA = m4 contrib/libdhash.spec.in

dist_include_HEADERS = dhash.h

lib_LTLIBRARIES = libdhash.la
libdhash_la_SOURCES = dhash.c
libdhash_la_LDFLAGS = \
    -version-info 1:0:0

check_PROGRAMS = dhash_test dhash_example
TESTS = $(check_PROGRAMS)

dhash_test_SOURCES = examples/dhash_test.c
dhash_test_LDADD = dhash.o

dhash_example_SOURCES = examples/dhash_example.c
dhash_example_LDADD = dhash.o

examplesdir = $(docdir)/examples
dist_examples_DATA = examples/dhash_test.c examples/dhash_example.c
dist_doc_DATA = README

tests: all $(check_PROGRAMS)

builddir ?= .
RPMBUILD ?= $(PWD)/rpmbuild

rpmroot:
	mkdir -p $(RPMBUILD)/BUILD
	mkdir -p $(RPMBUILD)/RPMS
	mkdir -p $(RPMBUILD)/SOURCES
	mkdir -p $(RPMBUILD)/SPECS
	mkdir -p $(RPMBUILD)/SRPMS

rpms: dist-gzip rpmroot
	cp $(builddir)/contrib/libdhash.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" -ba SPECS/libdhash.spec

# make srpms will use the old digest algorithm to be compatible
# with RHEL5
srpm: dist-gzip rpmroot
	cp $(builddir)/contrib/libdhash.spec $(RPMBUILD)/SPECS
	cp $(distdir).tar.gz $(RPMBUILD)/SOURCES
	cd $(RPMBUILD); \
	rpmbuild --define "_topdir $(RPMBUILD)" \
	         --define _source_filedigest_algorithm=1 \
	         -bs SPECS/libdhash.spec

