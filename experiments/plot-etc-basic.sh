#!/bin/bash

nth() {
  ls results/*/$2/fb_etc.csv|sort -r|sed -n "$1{p;q;}"
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

for i in "$@"; do
  SHORT_TITLES_50TH="$SHORT_TITLES_50TH ${i}_50th"
  SHORT_TITLES_99TH="$SHORT_TITLES_99TH ${i}_99th"
done

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-10`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10`"
  SHORT_TITLES_50TH="Linux-50th IX-50th"
  SHORT_TITLES_99TH="Linux-99th IX-99th"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
  copy_files ../papers/osdi14/figs/data/memcached $SHORT_FILES
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title1='"'"$SHORT_TITLES_50TH"'"' -e title3='"'"$SHORT_TITLES_99TH"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/memcached-etc-basic.$FORMAT"'"'       plot-etc-usr-basic.gnuplot
if [ $FORMAT = 'eps' ]; then
  gnuplot -e title1='"'"$SHORT_TITLES_50TH"'"' -e title3='"'"$SHORT_TITLES_99TH"'"' -e outfile='"'"$OUTDIR/memcached-key.eps"'"' plot-etc-usr-basic-key.gnuplot
fi
