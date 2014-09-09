#!/bin/bash

last() {
  ls results/*/IX-10/fb_usr$1.csv|sort -r|head -1
}
dir=../papers/osdi14/figs/data/batch_mutilate
mkdir -p $dir
title="1 16 64"
for i in _b1 _b16 ""; do
  file=`last $i`
  cp -a $file $dir
  infile="$infile $file"
done

echo 'Using files:'
echo $infile|tr ' ' '\n'

gnuplot -e infile='"'"$infile"'"' -e title='"'"$title"'"' plot_batch_mutilate.gnuplot
