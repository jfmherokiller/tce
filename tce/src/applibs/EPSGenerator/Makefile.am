noinst_LTLIBRARIES = libepsgenerator.la
libepsgenerator_la_SOURCES = EPSGenerator.cc VertexList.cc

SRC_ROOT = $(top_srcdir)/src

TOOLS_DIR = ${SRC_ROOT}/tools
AM_CPPFLAGS = -I${TOOLS_DIR}

libepsgenerator_la_LDFLAGS = 

MAINTAINERCLEANFILES = *~ *.gcov *.bbg *.bb *.da




## headers start
libepsgenerator_la_SOURCES += \
	VertexList.hh EPSGenerator.hh 
## headers end
