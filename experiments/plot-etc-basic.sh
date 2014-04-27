#!/bin/bash

nth() {
  ls results/*/$2/fb_etc.csv|sort -r|sed -n "$1{p;q;}"
}

FORMAT=${1:-eps}

shift

SHORT_FILES="$@"
SHORT_TITLES="$@"

if [ -z "$SHORT_FILES" ]; then
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-10`"
  SHORT_FILES="$SHORT_FILES `nth 1 Linux-40`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-10`"
  SHORT_FILES="$SHORT_FILES `nth 1 IX-40`"
  SHORT_TITLES_AVE="Linux-10Gbps-Median Linux-40Gbps-Median IX-10Gbps-Median IX-40Gbps-Median"
  SHORT_TITLES_95TH="Linux-10Gbps-95th Linux-40Gbps-95th IX-10Gbps-95th IX-40Gbps-95th"
fi

echo 'Using files:'
echo $SHORT_FILES|tr ' ' '\n'

if [ $FORMAT = 'eps' ]; then
  OUTDIR=../papers/osdi14/figs
else
  OUTDIR=figures
fi

gnuplot -e format='"'"$FORMAT"'"' -e title1='"'"$SHORT_TITLES_AVE"'"' -e title2='"'"$SHORT_TITLES_95TH"'"' -e infile='"'"$SHORT_FILES"'"' -e outfile='"'"$OUTDIR/memcached-etc-basic.$FORMAT"'"'       plot-etc-usr-basic.gnuplot
