# messages/sec VS message size
# 999999999999 message per connection

set terminal pngcairo dashed size 1680,1050
set style data linespoints
set output outfile
set grid y
set ylabel 'Throughput (Mbps)'
set xlabel 'Message Size'
#set xrange [0:1.6]
#set yrange [0:160]
plot for [i = 1:words(infile)] word(infile,i) using 1:($4*$1*8/10**6) title word(title,i)
