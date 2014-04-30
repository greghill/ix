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
plot for [i=1:words(title1)] NaN with linespoints title word(title1,i) linestyle i, for [i=1:words(title2)] NaN with linespoints title word(title2,i) linestyle i+4
