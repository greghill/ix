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
  SHORT_TITLES_AVE="$SHORT_TITLES_AVE ${i}_median"
  SHORT_TITLES_95TH="$SHORT_TITLES_95TH ${i}_95th"
done

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-10`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10`"
  SHORT_TITLES_AVE="Linux-10Gbps-Median IX-10Gbps-Median"
  SHORT_TITLES_95TH="Linux-10Gbps-95th IX-10Gbps-95th"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
  copy_files ../papers/osdi14/figs/data/memcached $SHORT_FILES
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title1='"'"$SHORT_TITLES_AVE"'"' -e title2='"'"$SHORT_TITLES_95TH"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/memcached-etc-basic.$FORMAT"'"'       plot-etc-usr-basic.gnuplot
