if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 2.1,1.4 font 'Times'
  unset key
  gen_title(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title(i) = word(title,i)
}
set style data linespoints
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'blue'
set style line 3 linecolor rgbcolor 'black'
set output outfile
set grid y
set border 3
set tics out nomirror

fig(infile) = "< awk '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
set xlabel 'Number of CPU cores'
set ylabel 'Messages/sec (x 10^{6})'
set xrange [0:*]
set yrange [0:*]
plot for [i=1:words(infile)] fig(word(infile,i)) using 1:($4/10**6) title gen_title(i) linestyle i
