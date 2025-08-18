SUBSRC1 = $(wildcard src/*.cc)
SUBOBJ = $(SUBSRC1:.cc=.o)
SUBHDR = $(wildcard include/*.hh) 
SUBTHDR = $(wildcard include/T*.hh) 

AUTOMAKE_OPTIONS = foreign

INCLUDES = -Iinclude -I$(ROOTSYS)/include -I$(TARTSYS)/include
CXXFLAGS = -fPIC -Iinclude $(shell root-config --cflags) -I$(TARTSYS)/include -g -Wall
LDFLAGS = $(shell root-config --glibs)

include_HEADERS = $(SUBHDR) $(SUBHDRF)

lib_LTLIBRARIES = libsmdata.la
libsmdata_la_SOURCES = $(SUBSRC1) smdata_dict.cc
libsmdata_la_DEPENDENCIES = $(SUBOBJ)
libsmdata_la_LIBADD = $(SUBOBJ)
libsmdata_la_LDFLAGS = $(LDFLAGS)

smdata_dict.cc: $(SUBTHDR) smdata_linkdef.hh
	rootcint -f $@ -c $(DEFAULT_INCLUDES) $(INCLUDES) $^

.cc.o:

.f.o:

.c.o:

clean-local:
	rm -f *dict*

clean:
	rm -f $(SUBOBJ)
