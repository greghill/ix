#!/bin/bash

nth() {
  ls results/*/pingpong/$2/data|sort -r|sed -n "$1{p;q;}"
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

SHORT_FILES="$@"
SHORT_TITLES="$@"

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Netpipe-mTCP/Netpipe-mTCP`"
  SHORT_FILES="$SHORT_FILES `nth 1 Netpipe-10/Netpipe`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10-RPC/IX`"
  SHORT_TITLES="mTCP-mTCP Linux-Linux IX-IX"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
  copy_files ../papers/osdi14/figs/data $SHORT_FILES
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title='"'"$SHORT_TITLES"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/pingpong.$FORMAT"'"' plot-pingpong-with-mtcp.gnuplot
