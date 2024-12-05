#!/bin/sh

if [ $# -ne 2 ] || ! [ -d $1 ]; then
  echo "Usage: $0 <DIRECTORY> <SEARCH_STRING>"
  exit 1
fi

FILES=$(find $1 -type f | wc -l)
MATCHES=$(grep -r $2 $1 | wc -l)
echo "The number of files are $FILES and the number of matching lines are $MATCHES"