#!/bin/bash

if [ $# -lt 5 ]
then
    echo "call as: generate.sh problem_no num_nodes num_arcs min_cap max_cap [outfile]"
    exit
fi

PROBLEM_NO=$1
NUM_NODES=$2
NUM_ARCS=$3
MIN_CAP=$4
MAX_CAP=$5

OUT_FILE=$6

SEED=`date "+%s"`

NETGEN_INPUT_STRING=$SEED"\n$PROBLEM_NO\n$NUM_NODES\n1\n1\n$NUM_ARCS\n1\n1\n1\n0\n0\n0\n100\n$MIN_CAP\n$MAX_CAP\na"
# sondaki 'a' ya takilma
if [ $# -gt 5 ]
then
    EXEC="echo -e \"$NETGEN_INPUT_STRING\" | netgen > $OUT_FILE"
    #`echo -e $NETGEN_INPUT_STRING | ./netgen > $OUT_FILE`
else
    EXEC="echo -e \"$NETGEN_INPUT_STRING\" | netgen>&1"
    #`echo -e $NETGEN_INPUT_STRING | ./netgen>&1`
fi

eval $EXEC

