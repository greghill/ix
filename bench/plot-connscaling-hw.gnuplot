if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.4,2.1 font 'Times'
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
}
set output outfile
set style data linespoints
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'blue'
set style line 3 linecolor rgbcolor 'black'
set grid y
set border 11
set tics out nomirror
set key top right
set xrange [*:300000]
set yrange [0:12]
set y2range [0:80]
set y2tics
set logscale x
set xlabel 'Connection Count (log scale)'
set ylabel 'cycles per msg (x 10^{3})'
plot infile using 1:($5/10**3) title 'cycles per msg' linestyle 1, \
     ''     using 1:7 axes x1y2 title 'L3 misses per msg' linestyle 2, \
     ''     using 1:9 axes x1y2 title 'avg. batch size' linestyle 3
