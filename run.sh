#!/bin/bash

# Switch to script directory
cd `dirname -- "$0"`

if [[ "$1" == '-port' ]]; then
	shift
	export DISPLAY="$1:"
	echo "change to display channel $DISPLAY"
	shift
fi

TEST=$1
shift

export OMP_NUM_THREADS=1

if [[ "$TEST" == 'basic_test' ]]; then
  th torcs_test.lua

else
  echo "Invalid options"
fi
