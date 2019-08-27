#!/bin/bash

testDir=$1

passed=true

for f in $testDir/*
do
    echo $f
    fRun=${f/regression/output}
    tc_passed=true
    
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
	rv=$?
        if [[ $rv != 0 ]]
        then
	    before_val=0
	    after_val=0
	    has_non_recv_diffline=false
            test_diff=$(echo "$test_diff" | tail -n +5)
            while IFS= read -r line;
            do
                tmp_float_num=-1
		is_recv_diffline=false
                # grep returns 0 if recv_time is present prefixed with + or - at the start of the line (if not, ignore)
		echo "$line" | grep "^[+-]recv_time";
		rv=$?
                if [[ $rv == 0 ]]
                then
		    is_recv_diffline=true
		    # remove everything but the number following a "recv_time:" prefixed by a single character
                    tmp_float_num=${line##?recv_time:}
                    # grep returns 1 if there are only 0-9 and . in the string (fail if other content)
                    if echo "$tmp_float_num" | grep [^0-9.];
                    then
                        tc_passed=false
                    fi
                fi
		
		# check to make sure that the diff isn't for something other than recv_time
		echo "$line" | grep "^[+-]";
		rv=$?
		if [[ $rv == 0 && $is_recv_diffline != "true" ]];
		then
		    has_non_recv_diffline=true
		fi
		
                if [[ $line == "-"* ]]
                then
		    before_val=$tmp_float_num
		    if [[ $is_recv_diffline != "true" ]]
		    then
		    	tc_passed=false
		    fi
                elif [[ $line == "+"* ]]
                then
		    after_val=$tmp_float_num
		    if [[ $is_recv_diffline != "true" ]]
		    then
			tc_passed=false
		    fi
                elif [[ $line == "~" ]]
                then
		    echo "Checking difference..."
		    if [[ $has_non_recv_diffline == "false" ]];
		    then
		        echo "- checking recv_time diff [$before_val vs $after_val]"
		    	if [[ $(echo "sqrt(($after_val - $before_val)^2) > 0.000001" | bc -l) == 1 ]]
		    	then
			    tc_passed=false
			    echo "-- outside tolerance [> .000001]"
			else
			    echo "-- within tolerance [<= .000001]"
		        fi
		    else
			tc_passed=false
			echo "- diff contained a change that wasn't recv_time"
		    fi
		    # reset flag for detecting a non-recv_time change
		    has_nonrecv_diffline=false
                fi
            done <<< "$test_diff"

	    if [[ $tc_passed == false ]];
	    then
	    	passed=false
            	echo "FAILED $f does not match"
	    fi
        fi
    esac
done

if [ $passed = "true" ]
then
   exit 0
else
   exit 1
fi
