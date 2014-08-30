set terminal postscript eps enhanced solid lw 1 font 'Times'
set style line 1 pointtype 4 linecolor rgbcolor 'red'
set style line 2 pointtype 4 linecolor rgbcolor 'black'
set style line 3 pointtype 6 linecolor rgbcolor 'red'
set style line 4 pointtype 6 linecolor rgbcolor 'black'
set output outfile

unset border
unset tics
set key box horizontal reverse center top height 0.3 width -5
set yrange [-1:1]
set size 1.2,.1
plot NaN with linespoints linestyle 1 title 'Linux (avg)',\
     NaN with linespoints linestyle 3 title 'Linux (99^{th} pct)',\
     NaN with linespoints linestyle 2 title 'IX (avg)',\
     NaN with linespoints linestyle 4 title 'IX (99^{th} pct)'
