#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Usage: $0 <FILE> <CONTENTS>"
  exit 1
fi

mkdir -p $(dirname $1) && echo $2 > $1
