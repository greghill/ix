#!/bin/bash

nth() {
  ls results/*/memcached-pareto/$2/mutilate.out|sort -r|sed -n "$1{p;q;}"
}

copy_files() {
  DEST=$1
  mkdir -p $DEST
  shift
  while [ $# -gt 0 ]; do
    FILE=$1
    tar -c `dirname $FILE` | tar --strip-components=2 -C $DEST -x
    shift
  done
}

FORMAT=${1:-eps}

shift

FILE_LINUX="`nth 1 Linux`"
FILE_IX="`nth 1 IX`"

echo 'Using files:'
echo $FILE_LINUX
echo $FILE_IX

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/atc15/figs
  copy_files ../papers/atc15/figs/data $FILE_LINUX $FILE_IX
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e infile='"'"$FILE_LINUX"'"' -e outfile='"'"$OUTDIR/pareto-eff-linux.$FORMAT"'"'       plot-pareto-eff.gnuplot
gnuplot -e format='"'"$FORMAT"'"' -e infile='"'"$FILE_IX"'"' -e outfile='"'"$OUTDIR/pareto-eff-ix.$FORMAT"'"'       plot-pareto-eff.gnuplot
