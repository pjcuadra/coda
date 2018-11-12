## Process this file with automake to produce Makefile.in

bin_PROGRAMS = unit	

UTIL_TESTS = util/u_bitmap.cc

unit_SOURCES = main.cc $(UTIL_TESTS)

unit_LDADD = $(top_builddir)/coda-src/util/libutil.la \
             $(top_builddir)/lib-src/base/libbase.la \
             $(RVM_RPC2_LIBS)

AM_LDFLAGS = -lgtest -pthread

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_builddir)/coda-src

LIBTOOL = libtool --mode=execute
GTEST_FLAGS = --gtest_output="xml:$(top_builddir)/unittest.xml" --gtest_shuffle
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all
UNIT_BIN = $(top_builddir)/test-src/unit/unit

all: unit

check-local: all
	${LIBTOOL} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}

memcheck: all
	${LIBTOOL} valgrind ${VALGRIND_FLAGS} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}
