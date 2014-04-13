if (format eq 'eps') {
  set terminal postscript eps enhanced lw 1 font 'Times'
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
set tics out nomirror font ',24'

fig(infile) = "< awk '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
set xlabel 'Number of CPU threads' font ',24'
set ylabel 'Messages/sec (x 10^{6})' font ',24'
set xrange [0:*]
set yrange [0:*]
set xtics ('0' 0)
plot for [i=1:words(infile)] fig(word(infile,i)) using ($0+1):($4/10**6):xticlabel(1) title gen_title(i)
