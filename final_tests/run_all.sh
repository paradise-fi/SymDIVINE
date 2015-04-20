#!/bin/bash

trap "echo Exited!; exit;" SIGINT SIGTERM

DIR=$1

for prog in $(find $DIR -name '*.ll'); do
    if $(test ! -f $prog.result); then
        ./run_test.sh $prog || break;
    fi
done

