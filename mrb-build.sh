#!/bin/sh
cd vendor/mruby/
make CFLAGS="-g -fPIC -O0 -DGC_DEBUG"
