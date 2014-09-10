if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.1,2.1 font 'Times'
  unset key
  gen_title(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title(i) = word(title,i)
}
set style data linespoints
set style line 1 pointtype 6 linecolor rgbcolor 'red'
set style line 2 pointtype 7 linecolor rgbcolor 'red'
set style line 3 pointtype 8 linecolor rgbcolor 'blue'
set style line 4 pointtype 4 linecolor rgbcolor 'black'
set style line 5 pointtype 5 linecolor rgbcolor 'black'
set output outfile
set grid y
set border 3
set tics out nomirror

fig(infile) = "< awk '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
set xlabel 'Number of CPU cores'
set ylabel 'Messages/sec (x 10^{6})'
set xrange [0:*]
set yrange [0:*]
set xtics ('0' 0, '1' 2, '2' 4, '3' 6, '4' 8, '5' 10, '6' 12, '7' 14, '8' 16)
plot for [i=1:words(infile)] fig(word(infile,i)) using 1:($4/10**6) title gen_title(i) linestyle i
