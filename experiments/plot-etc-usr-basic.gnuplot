if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.1,2.1 font 'Times'
  unset key
  gen_title1(i) = ''
  gen_title3(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title1(i) = word(title1,i)
  gen_title3(i) = word(title3,i)
  set key left top
}
set style data linespoints
set style line 1 pointtype 4 pointsize .5 linecolor rgbcolor 'red'
set style line 2 pointtype 4 pointsize .5 linecolor rgbcolor 'black'
set style line 3 pointtype 6 pointsize .5 linecolor rgbcolor 'red'
set style line 4 pointtype 6 pointsize .5 linecolor rgbcolor 'black'
set output outfile
set grid y
set border 3
set tics out nomirror

set datafile separator ","
set xlabel xlabel . 'Throughput (RPS x 10^{3})'
set ylabel 'Latency ({/Symbol m}s)'
set xrange [0:2000000]
set yrange [0:750]
set xtics ( "0" 0, "250" 250000, "500" 500000, "750" 750000, "1000" 1000000, "1250" 1250000, "1500" 1500000, "1750" 1750000, "2000" 2000000, "2250" 2250000, "2500" 2500000, "2750" 2750000, "3000" 3000000 )
set ytics (0, 250, 500, 750, 1000)
set arrow from 0,500 to 1850000,500 nohead lw 1
set label 'SLA' at 1860000,500
plot for [i=1:words(infile)] word(infile,i) using 11:2 title gen_title1(i) linestyle i, \
     for [i=1:words(infile)] word(infile,i) using 11:10 title gen_title3(i) linestyle i+2

