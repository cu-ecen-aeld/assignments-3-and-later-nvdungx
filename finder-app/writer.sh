#!/bin/bash

set -e
set -u

writefile=$1
writestr=$2

if [ $# -lt 2 ]; then
    echo "Error: need 2 inputs 'writefile' and 'writestr'"
    exit 1
fi

writedir=$(dirname "$writefile")
mkdir -p "$writedir"
if [ $? -ne 0 ]; then
    echo "Error: Could not create directory path $writedir"
    exit 1
fi

echo $writestr > $writefile
if [ $? -ne 0 ]; then
    echo "Error: Could not create file $writefile"
    exit 1
fi
echo "File created successfully: $writefile"
