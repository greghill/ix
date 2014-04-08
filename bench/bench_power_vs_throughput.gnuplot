# average power VS throughput
# CPU threads = 16
# 64 bytes per message
# 1 message per connection

set terminal pngcairo dashed size 1680,1050
set style data linespoints
set output outfile
set grid y
set ylabel 'Average power (W)'
set xlabel 'Messages/sec (x 10^6)'
set xrange [0:1.6]
set yrange [0:160]
plot for [i = 1:words(infile)] word(infile,i) using ($3/10**6):(($11+$12)/60) title word(title,i)
