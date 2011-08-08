#!/bin/sh
make clean
make
LD_LIBRARY_PATH=/usr/local/lib gdb --ex run ./test
