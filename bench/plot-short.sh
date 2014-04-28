#!/bin/bash

nth() {
  ls results/*/short/$2/data|sort -r|sed -n "$1{p;q;}"
}

FORMAT=${1:-eps}

shift

SHORT_FILES="$@"
SHORT_TITLES="$@"

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-40-RPC/Linux-Libevent`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10-RPC/Linux-Libevent`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-40-RPC/Linux-Libevent`"
  SHORT_TITLES="Linux-40Gbps IX-10Gbps IX-40Gbps"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-mcore40.$FORMAT"'"'       plot-short-mcore.gnuplot
gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-roundtrips40.$FORMAT"'"'  plot-short-roundtrips.gnuplot
gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-size40.$FORMAT"'"'        plot-short-size.gnuplot
if [ $FORMAT = 'eps' ]; then
  gnuplot -e title='"'"$SHORT_TITLES"'"' -e outfile='"'"$OUTDIR/short-key40.eps"'"' plot-short-key.gnuplot
fi
