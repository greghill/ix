set terminal postscript eps enhanced lw 1 font 'Times'
set style data linespoints
set output outfile
set grid y
unset key
set border 3
set tics out nomirror font ',24'

fig(infile) = "< awk '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
set xlabel 'Number of CPU threads' font ',24'
set ylabel 'Messages/sec (x 10^{6})' font ',24'
set xrange [0:*]
set xtics ('0' 0)
plot for [f in infile] fig(f) using ($0+1):($6/10**6):xticlabel(1) lw 7
