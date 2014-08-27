#!/bin/bash

nth() {
  ls results/*/connscaling/$2/data|sort -r|sed -n "$1{p;q;}"
}

data=../papers/osdi14/figs/data/connscaling/IX-40-RPC/Linux-Libevent/hw
tmp2=`mktemp`
egrep 'BEGIN|starting' ../ix/ix.log | \
  sed -e 's/.*starting.*/--/' \
      -e 's/.* \([0-9]*\)% idle, \([0-9]*\)% user, \([0-9]*\)% sys, non idle cycles=\([0-9]*\), HW instructions=\([0-9]*\), LLC load misses=\([0-9]*\) (\([0-9]*\) pkts, avg batch=\([0-9]*\) \[\], avg backlog=\([0-9]*\).*/\1\t\2\t\3\t\4\t\5\t\6\t\7\t\8\t\9/' \
      -e '$a--' | \
  awk '/--/{
    if (line==0)
      next
    for (i=1;i<10;i++)
      s[i]/=line-17
    printf "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f\n", s[1], s[2], s[3], s[4]/s[7], s[5]/s[7], s[6]/s[7], s[7]/5*16, s[8], s[9]
    for (i=1;i<10;i++)
      s[i]=0
    line=0
  }
  //{
    line++
    if (line>17)
      for (i=1; i<=NF; i++)
        s[i]+=$i
  }' > $tmp2
paste <(cut -f1 `nth 1 IX-40-RPC/Linux-Libevent`) $tmp2 > $data
rm $tmp2
gnuplot <<EOF
set terminal pngcairo size 1280,1024 lw 1 font 'Times'
set output 'figures/connscaling-hw-all.png'
set style data linespoints
set grid y
set border 3
set tics out nomirror
set key top left invert
set xrange [*:300000]
set yrange [0:*]
set logscale x
set multiplot layout 3,3
plot '$data' using 1:2 title 'Idle (%)', \
     ''      using 1:3 title 'User (%)', \
     ''      using 1:4 title 'System (%)'
plot '$data' using 1:5 title 'Non-idle cycles/packet'
set yrange [0:4000]
#set logscale y
plot '$data' using 1:6 title 'HW instructions/packet'
set yrange [0:*]
#unset logscale y
plot '$data' using 1:7 title 'LLC load misses/packet'
set xlabel 'Connection Count (log scale)'
plot '$data' using 1:(\$8/10**6) title 'Packets/second (10^6)'
plot '$data' using 1:9 title 'Average batch size (# packets)'
plot '$data' using 1:10 title 'Average backlog (# batches)'
EOF
gnuplot -e format='"eps"' -e infile='"'"$data"'"' -e outfile='"../papers/osdi14/figs/connscaling-hw.eps"' plot-connscaling-hw.gnuplot
gnuplot -e format='"png"' -e infile='"'"$data"'"' -e outfile='"figures/connscaling-hw.png"' plot-connscaling-hw.gnuplot
