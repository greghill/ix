if (format eq 'eps') {
  set terminal postscript eps enhanced size 2.1,1.4 font 'Times'
  unset key
  gen_title1(i) = ''
  gen_title2(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title1(i) = word(title1,i)
  gen_title2(i) = word(title2,i)
  set key left top
}
set style data linespoints
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'orange'
set style line 3 linecolor rgbcolor 'blue'
set style line 4 linecolor rgbcolor 'green'
set style line 5 linecolor rgbcolor 'red'
set style line 6 linecolor rgbcolor 'orange'
set style line 7 linecolor rgbcolor 'blue'
set style line 8 linecolor rgbcolor 'green'
set output outfile
set grid y
set border 3
set tics out nomirror

set datafile separator ","
set xlabel 'Throughput (QPS x 10^{3})'
set ylabel 'Latency ({/Symbol m}s)'
set xrange [0:2000000]
set yrange [0:3000]
set xtics ( "0" 0, "250" 250000, "500" 500000, "750" 750000, "1000" 1000000, "1250" 1250000, "1500" 1500000, "1750" 1750000, "2000" 2000000, "2250" 2250000, "2500" 2500000, "2750" 2750000, "3000" 3000000 )
plot for [i=1:words(infile)] word(infile,i) using 11:7 title gen_title1(i) linestyle i, for [i=1:words(infile)] word(infile,i) using 11:9 title gen_title2(i) linestyle i+4
