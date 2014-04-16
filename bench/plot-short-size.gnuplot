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

fig(infile) = "< awk '//{if ($1==16&&$3==1)print $0 }' ".infile.'| sort -nk2'
set xlabel 'Message Size' font ',24'
set ylabel 'Throughput (Gbps)' font ',24'
set xrange [0:*]
set yrange [0:*]
set xtics ('0' 0)
set label 'line rate @ 10GbE' at 0, 9.57 offset character 2, .5
set label 'line rate @ 4x10GbE' at 0, 38.27 offset character 2, .5
plot for [i=1:words(infile)] fig(word(infile,i)) using ($0+1):($2*$4*8/10**9):xticlabel(2) title gen_title(i), \
  9.57 title '', \
  38.27 title ''
