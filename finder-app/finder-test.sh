#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# Reference: https://stackoverflow.com/questions/592620/how-can-i-check-if-a-program-exists-from-a-bash-script
# Below we are checking whether the program is in the PATH of the 
# use the appropriate finder utility
if ! command -v writer &> /dev/null; then
  # the finder-test script is probably running from build machine
  WRITER_UTILITY="./writer"
else
  # the finder-test script is probably running on the target
  # and thus exists in the PATH
  WRITER_UTILITY="writer"
fi

# use the appropriate finder utility
if ! command -v finder.sh &> /dev/null; then
  # the finder-test script is probably running from build machine
  FINDER_UTILITY="./finder.sh"
else
  # the finder-test script is probably running from the target
  # and thus exists in the PATH
  FINDER_UTILITY="finder.sh"
fi

# find where we have stored the conf/username.txt 
if [ -e ./conf/username.txt ]; then
  username=$(cat conf/username.txt)
else
  username=$(cat /etc/finder-app/conf/username.txt)
fi

if [ $# -lt 2 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"
mkdir -p "$WRITEDIR"

#The WRITEDIR is in quotes because if the directory path consists of spaces, then variable substitution will consider it as multiple argument.
#The quotes signify that the entire string in WRITEDIR is a single string.
#This issue can also be resolved by using double square brackets i.e [[ ]] instead of using quotes.
if [ -d "$WRITEDIR" ]
then
	echo "$WRITEDIR created"
else
	exit 1
fi

# A3-P1 instructions, removed make step
#echo "Removing the old writer utility and compiling as a native application"
#make clean
#make

for i in $( seq 1 $NUMFILES)
do
  ${WRITER_UTILITY} "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

# record output from finder utility and post result to /tmp/assignment-4-result.txt
OUTPUTSTRING=$(${FINDER_UTILITY} "$WRITEDIR" "$WRITESTR")
echo ${OUTPUTSTRING} > /tmp/assignment-4-result.txt

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi
