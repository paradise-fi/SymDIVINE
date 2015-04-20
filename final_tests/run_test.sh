#!/bin/bash

TIMEOUT=900 # 15min
MEMORY=15000000 # 15 GB

REACHABILITY_BIN="../../symdivine/reachability"
LLPROGRAM=$1
TIME_FILE=$LLPROGRAM.time
OUTPUT_FILE=$LLPROGRAM.output
RESULT_FILE=$LLPROGRAM.result

if $(test ! -f $LLPROGRAM); then
    echo the file $LLPROGRAM does not exist!;
    exit
fi

(ulimit -t $TIMEOUT -v $MEMORY; /usr/bin/time -o $TIME_FILE -f '%U' $REACHABILITY_BIN  $LLPROGRAM -s ) > $OUTPUT_FILE

echo "Unknown" > $RESULT_FILE

echo $LLPROGRAM
cat $OUTPUT_FILE | grep Safe && echo "Safe" > $RESULT_FILE
cat $OUTPUT_FILE | grep "is reachable" && echo "Unsafe" > $RESULT_FILE

exit 0

