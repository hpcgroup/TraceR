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
        test_diff=$(git diff -U0 --word-diff=porcelain --no-index -- $fRun $f)
        if ! $?
        then
            test_diff=$(echo "$test_diff" | tail -n +5)
            while IFS= read -r line;
            do
                echo "line=$line"
                tmp_float_num=-1
                # grep returns 0 if recv_time is present
                if echo "$line" | grep "recv_time"
                then
                    # remove everything but the number following "recv_time:"
                    tmp_float_num=${line##recv_time:}
                    # grep returns 1 if there are only 0-9 and . in the string (fail if other content)
                    if echo "$tmp_float_num" | grep [^0-9.]
                    then
                        passed=false
                    fi
                    # should set a flag in the else case for this, and if line was added/removed without this present in diff then fail
                    # reset flag variables in ~ part of else check
                fi
                if [[ $line == "-"* ]]
                then
                    echo "Removed diff line with value $tmp_float_num"
                elif [[ $line == "+"* ]]
                then
                    echo "Added diff line with value $tmp_float_num"
                elif [[ $line == "~" ]]
                then
                    echo "End of diff"
                fi
            done <<< "$test_diff"

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

