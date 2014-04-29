#!/bin/bash

nth() {
  ls results/*/pingpong/$2/data|sort -r|sed -n "$1{p;q;}"
}

FORMAT=${1:-eps}

shift

SHORT_FILES="$@"
SHORT_TITLES="$@"

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Netpipe-10/Netpipe`"
  SHORT_FILES="$SHORT_FILES `nth 1 Netpipe-mTCP/Netpipe`"
  SHORT_FILES="$SHORT_FILES `nth 1 Netpipe-mTCP/Netpipe-mTCP`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10-RPC/Linux-Libevent`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10-RPC/IX`"
  SHORT_TITLES="Linux-10Gbps mTCP-10Gbps mTCP-10Gbps-mTCP IX-10Gbps IX-10Gbps-IX"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/pingpong.$FORMAT"'"' plot-pingpong-with-mtcp.gnuplot
