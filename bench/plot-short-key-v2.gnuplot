set terminal postscript eps enhanced solid lw 1 font 'Times'
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'red'
set style line 3 linecolor rgbcolor 'blue'
set style line 4 linecolor rgbcolor 'black'
set style line 5 linecolor rgbcolor 'black'
set output outfile

unset border
unset tics
set key box horizontal left top height 0.3 width -2
set yrange [-1:1]
set size 0.7,.2
plot NaN with linespoints linestyle 1 title 'Linux 10Gbps',\
     NaN with linespoints linestyle 2 title 'Linux 40Gbps',\
     NaN with linespoints linestyle 4 title 'IX 10Gbps',\
     NaN with linespoints linestyle 5 title 'IX 40Gbps',\
     NaN with linespoints linestyle 3 title 'mTCP 10Gbps'
