#!/bin/bash

if [ $# -le 0 ]; then
  echo "Usage: `basename $0` CPU..." >&2
  echo "Prints total interrupts for all CPUs provided as command line arguments" >&2
  exit 1
fi

p="0"
while [ $# -gt 0 ]; do
  p="$p+\$$[2+$1]"
  shift
done
awk "//{s+=$p}END{print s}" /proc/interrupts
