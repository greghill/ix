set terminal postscript eps enhanced lw 1 font 'Times'
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'green'
set output outfile

unset border
unset tics
set key box horizontal reverse center top height 0.3
set yrange [-1:1]
set size 1,.1
plot for [i=1:words(title)] NaN with linespoints linestyle i title word(title,i)
