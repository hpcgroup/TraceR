#!/bin/bash

testDir=$1

passed=true

for f in $testDir/*
do
    echo $f
    fRun=${f/regression/output}

    case $f in
    *-msg-stats)
        # direct comparison won't work, times vary
	    ;;
    *~)
        continue
        ;;
    *)
        if ! git diff -U0 --word-diff=porcelain --no-index -- $fRun $f > /dev/null
        then
            passed=false
            echo "FAILED $f does not match"
        fi
    esac
done

if [ $passed = "true" ]
then
   exit 0
else
   exit 1
fi

