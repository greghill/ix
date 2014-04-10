set terminal postscript eps enhanced lw 1 font 'Times'
set style data linespoints
set output outfile
set grid y
unset key
set border 3
set tics out nomirror font ',24'

fig(infile) = "< awk '//{if ($1==16&&$3==1)print $0 }' ".infile.'| sort -nk2'
set xlabel 'Message Size' font ',24'
set ylabel 'Throughput (Gbps)' font ',24'
set xrange [0:*]
set xtics ('0' 0)
plot for [f in infile] fig(f) using ($0+1):($2*$6*8/10**9):xticlabel(2) lw 7
