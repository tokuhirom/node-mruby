#!/bin/sh
cd vendor/mruby/
# make directory to output binary files.
# mruby have a vendor/mruby/bin/.gitkeep, but npm packager ignores it and package tar ball doesn't contain it.
mkdir -p bin/
make CFLAGS="-g -fPIC -O0"
