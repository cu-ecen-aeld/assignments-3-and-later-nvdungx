#!/bin/sh

set -e
set -u

filesdir=$1
searchstr=$2

if [ $# -lt 2 ]; then
    echo "Error: need 2 inputs 'filesdir' and 'searchstr'"
    exit 1
fi

if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir does not exist"
    exit 1
fi

total_match_num=0
total_file_num=0

for item in $(grep -Frc "$searchstr" "$filesdir"); do
    file_match_num=${item##*:}

    if [ "$file_match_num" -ne 0 ]; then
        total_match_num=$((total_match_num + file_match_num))
        total_file_num=$((total_file_num + 1))
    fi
done

echo "The number of files are $total_file_num and the number of matching lines are $total_match_num"