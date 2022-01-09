#!/bin/bash
# --------------------------------------------------------------------------------
# Assignment 1 writer.sh script
# Make executable with chmod u+x and run with ./writer.sh <writefile> <writestr>
# Author: Jake Michael
# --------------------------------------------------------------------------------

# ensure 2 arguments are passed (filesdir and searchstr)
if [ $# -lt 2 ]; then
  echo "error: $# arguments given, 2 expected"
  exit 1
else
  WRITEFILE=$1
  WRITESTR=$2
fi

# make the directory / file and test if it exists
mkdir -p "${WRITEFILE%/*}" && touch "$WRITEFILE" 
if [[ ! -f $WRITEFILE ]]; then
  echo "$WRITEFILE unable to be created"
  exit 1
fi

# write the string to the new file (override file contents)
echo "$WRITESTR" > "$WRITEFILE" 

exit 0
