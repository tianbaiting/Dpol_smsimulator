#!/bin/sh

make clean
make distclean
rm ./*~ ./*/*~ ./*/*/*~ ./*/*/*/*~ ./*/*/*/*/*~
rm ./Makefile.in ./*/Makefile.in ./*/*/Makefile.in ./*/*/*/Makefile.in
rm ./aclocal.m4
rm -r ./autom4te.cache
rm ./config.guess
rm ./config.sub
rm ./configure
rm ./depcomp
rm -r ./include
rm ./install-sh
rm -r ./lib
rm ./ltmain.sh
rm ./missing
rm ./setup.sh