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
        echo "=========================================="
	echo "diff: $f"
	echo "=========================================="
        if ! diff $fRun $f
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

