#!/bin/bash

nth() {
  ls stanford_fig4_results/*/short/$2/data|sort -r|sed -n "$1{p;q;}"
}

FORMAT=${1:-eps}

shift

SHORT_FILES="$@"
SHORT_TITLES="$@"

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-10-RPC/Linux-Libevent`"
  SHORT_FILES="$SHORT_FILES `nth 1 mTCP-10-RPC/Linux-Libevent`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10-RPC/Linux-Libevent`"
  SHORT_TITLES="Linux-10Gbps mTCP-10Gbps IX-10Gbps"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-mcore10.$FORMAT"'"'       plot-short-mcore-with-mtcp.gnuplot
gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-roundtrips10.$FORMAT"'"'  plot-short-roundtrips-with-mtcp.gnuplot
gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/short-size10.$FORMAT"'"'        plot-short-size-with-mtcp.gnuplot
if [ $FORMAT = 'eps' ]; then
  gnuplot -e title='"'"$SHORT_TITLES"'"' -e outfile='"'"$OUTDIR/short-key10.eps"'"' plot-short-key-with-mtcp.gnuplot
fi
