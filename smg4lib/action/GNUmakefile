
name := smaction

CPPFLAGS += $(shell $(ROOTSYS)/bin/root-config --cflags) -g -I$(G4SMDATADIR)/include

#CPPFLAGS += $(shell $(ROOTSYS)/bin/root-config --cflags) -g -I$(G4SMDATADIR)/include -DDEBUG
LDLIBS += $(shell $(ROOTSYS)/bin/root-config --libs) -lsmdata

include ../GNUmakefile.in