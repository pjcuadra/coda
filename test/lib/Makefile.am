## Process this file with automake to produce Makefile.in

check_LTLIBRARIES = libgtest.la libgtest_main.la 

AM_CPPFLAGS = -isystem $(top_srcdir)/test/lib/googletest/googletest/include -I$(top_srcdir) $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS = $(GTEST_LDFLAGS) $(GTEST_LIBS) \
	     $(top_srcdir)/test/lib/googletest/googletest/libgtest_main.la \
	     $(top_srcdir)/test/lib/googletest/googletest/libgtest.la
       
SUBDIRS = googletest/googletest
