set terminal postscript eps enhanced solid lw 1 font 'Times'
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'black'
set style line 5 linecolor rgbcolor 'red'
set style line 6 linecolor rgbcolor 'black'
set output outfile

unset border
unset tics
set key box horizontal reverse center top height 0.3
set yrange [-1:1]
set size 1,.1
plot NaN with linespoints linestyle 1 title 'Linux 95^{th}',\
     NaN with linespoints linestyle 5 title 'Linux median',\
     NaN with linespoints linestyle 2 title 'IX 95^{th}',\
     NaN with linespoints linestyle 6 title 'IX median'
