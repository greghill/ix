if (format eq 'eps') {
  set terminal postscript eps enhanced size 3.1,2.1 font 'Times'
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
}
gen_title(i) = word(title,i)
set style data linespoints
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'green'
set output outfile
set grid y
set border 3
set tics out nomirror

set xlabel 'Connection Count (log scale)'
set ylabel 'Messages/sec (x 10^{6})'
set xrange [*:100000]
set yrange [0:*]
set logscale x
plot for [i=1:words(infile)] word(infile,i) using 1:($4/10**6) title gen_title(i) linestyle i
