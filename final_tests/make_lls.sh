#!/bin/sh

CFLAGS=-O2

for i in $(find -name '*.c'); do
    llname=$(echo $i | sed 's/.c$/.ll/');
    #if $(test ! -f $llname); then
        echo clang -S $CFLAGS -emit-llvm $i -o $llname;
        clang -S $CFLAGS -emit-llvm $i -o `echo $i | sed s/\.c\$/\.ll/g`;
    #fi
done
