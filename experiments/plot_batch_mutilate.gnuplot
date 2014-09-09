set terminal postscript eps enhanced size 3.1,2.1 font 'Times'
set style data lines
set output '../papers/osdi14/figs/batch-mutilate.eps'
set grid y
set border 3
set tics out nomirror
set datafile separator ","
set key left top

set xlabel 'USR: Throughput (RPS x 10^{3})'
set ylabel 'Latency ({/Symbol m}s)'
set xrange [0:2500000]
set yrange [0:750]
set xtics ( "0" 0, "250" 250000, "500" 500000, "750" 750000, "1000" 1000000, "1250" 1250000, "1500" 1500000, "1750" 1750000, "2000" 2000000, "2250" 2250000, "2500" 2500000, "2750" 2750000, "3000" 3000000 )
set arrow from 0,500 to 2350000,500 nohead lw 1
set label 'SLA' at 2360000,500
plot for [i=1:words(infile)] word(infile,i) using 11:10 title word(title,i)
