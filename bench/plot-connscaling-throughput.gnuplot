if (format eq 'eps') {
  set terminal postscript eps enhanced size 3.1,2.1 font 'Times'
  unset key
  gen_title(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title(i) = word(title,i)
}
set style data linespoints
set output outfile
set grid y
set border 3
set tics out nomirror

set xlabel 'Connection Count'
set ylabel 'Messages/sec (x 10^{6})'
set xrange [*:*]
set yrange [0:*]
set logscale x
plot for [i=1:words(infile)] word(infile,i) using 1:($4/10**6) title gen_title(i)
