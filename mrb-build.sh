#!/bin/sh
cd vendor/mruby/
make clean
make CFLAGS="-g -fPIC -O0"
