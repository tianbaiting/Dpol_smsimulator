# Makefile for PDCAnalysisTools library

# Get ROOT configuration
ROOTCFLAGS   := $(shell root-config --cflags)
ROOTLIBS     := $(shell root-config --libs)
ROOTGLIBS    := $(shell root-config --glibs)

# Compiler and flags
CXX          := g++
CXXFLAGS     := -fPIC -Wall $(ROOTCFLAGS)

# Library name
LIBNAME      := libPDCAnalysisTools.so

# Directories
SRCDIR       := src
INCDIR       := include
OBJDIR       := obj

# Source files and object files
SOURCES      := $(wildcard $(SRCDIR)/*.cc)
DICT_SRC     := $(SRCDIR)/Dict.cc
OBJECTS      := $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(filter-out $(DICT_SRC), $(SOURCES)))
DICT_OBJ     := $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(DICT_SRC))

# Targets
all: $(LIBNAME)

$(LIBNAME): $(OBJECTS) $(DICT_OBJ)
	@echo "---> Linking shared library $@..."
	@$(CXX) -shared -o $@ $^ $(ROOTLIBS)
	@echo "---> Library $(LIBNAME) created successfully."

$(DICT_SRC): $(INCDIR)/PDCRotation.hh $(INCDIR)/PDCSmearer.hh $(SRCDIR)/LinkDef.h
	@echo "---> Generating ROOT dictionary $@..."
	@rootcling -f $@ -c $(CXXFLAGS) -I$(INCDIR) $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cc
	@mkdir -p $(OBJDIR)
	@echo "---> Compiling $<..."
	@$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	@echo "---> Cleaning up..."
	@rm -rf $(OBJDIR) $(DICT_SRC) $(LIBNAME)
	@echo "---> Done."

.PHONY: all clean
