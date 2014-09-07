set terminal postscript eps enhanced solid size 3.1,1.5 font 'Times'
set style data linespoints
set output '../papers/osdi14/figs/batch.eps'
set grid y
set border 3
set tics out nomirror

set xlabel 'Batch size (packets)'
set ylabel 'Messages/sec (x 10^{6})'
set xrange [0:*]
set yrange [0:*]
plot '-' using ($0+1):($2/10**6):xtic(1) title ''
1       5389598
2       6996271
4       7951872
8       10195400
16      10788173
32      11722580
64      11669686
128     11859553
