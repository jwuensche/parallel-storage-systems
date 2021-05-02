#!/bin/env sh

setopt -e

make checkpoint
make checkpoint_fsync
make checkpoint_optimized

rm -r results
mkdir results

function measure() {
    # 1 - executable name
    # 2 - iteration count

    echo "Measuring ${1}"
    echo "===================="
    for thread in $(seq 1 12)
    do
        echo "Performing measure with ${thread} threads"
        if test -e "results/${1}.csv"
        then
            ./"${1}" "${thread}" "${2}" | tail -n +2 >> "results/${1}.csv"
        else
            ./"${1}" "${thread}" "${2}" > "results/${1}.csv"
        fi
    done
}


measure "checkpoint" 1000
measure "checkpoint_fsync" 10
measure "checkpoint_optimized" 100
