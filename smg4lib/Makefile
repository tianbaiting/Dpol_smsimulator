CC			 = $(shell root-config --cc)
CXX			 = $(shell root-config --cxx)

.PHONY: data physics construction action

all: data physics construction action
	@if [ ! -d ./include ] ; then mkdir ./include  ;fi
	cp ./*/include/* ./include
	@if [ ! -d ./lib ] ; then mkdir ./lib  ;fi
	cp ./*/lib/* ./lib
	cp ./data/sources/smdata_dict_rdict.pcm ./lib

action: data
	$(MAKE) -C $@
construction: data
	$(MAKE) -C $@
physics:
	$(MAKE) -C $@
data:
	cd data; if [ ! -e ./Makefile ] ; then ./autogen.sh --prefix=$(PWD)/data; fi; make install

clean:
	-$(MAKE) clean -C data
	$(MAKE) clean -C physics
	$(MAKE) clean -C construction
	$(MAKE) clean -C action
	$(MAKE) clean_lib -C physics
	$(MAKE) clean_lib -C construction
	$(MAKE) clean_lib -C action

cleanall: clean
	cd data; ./scripts/clean.sh; cd -
	-rm -v ./*~
	-rm -v ./*/*~
	-rm -v ./*/*/*~
	-rm -v -r ./*/tmp
	-rm -v -r ./*/bin
	-rm -v -r ./include
	-rm -v -r ./lib