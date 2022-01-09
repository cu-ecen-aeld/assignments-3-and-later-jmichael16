#!/bin/bash
# --------------------------------------------------------------------------------
# Assignment 1 finder.sh script
# Make executable with chmod u+x and run with ./finder.sh <filedir> <searchstr>
# Author: Jake Michael
# --------------------------------------------------------------------------------

# ensure 2 arguments are passed (filesdir and searchstr)
if [ $# -lt 2 ]; then
  echo "error: $# arguments given, 2 expected"
  exit 1
else
  FILESDIR=$1
  SEARCHSTR=$2
fi

# check if filesdir exists on filesystem
if [[ ! -d $FILESDIR ]]; then
  echo "$FILESDIR is not a directory or does not exist"
  exit 1
fi

# use find command with pipe to wc (to count lines)
NUM_FILES=$(find "$FILESDIR" -type f | wc -l) 

# use grep command -r (recursive) with pipe to wc (to count lines)
NUM_LINES=$(grep -re "$SEARCHSTR" "$FILESDIR" | wc -l)

echo "The number of files are $NUM_FILES and the number of matching lines are $NUM_LINES"

exit 0
