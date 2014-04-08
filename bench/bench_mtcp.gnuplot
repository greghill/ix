# fig1
# messages/sec VS CPU threads
# CPU threads = [1..16]
# 64 bytes per message
# 1 message per connection

# fig2
# messages/sec VS messages per connection
# 16 CPU threads
# 64 bytes per message
# messages per connection = [1,2,8,32,64,128]

# fig3
# messages/sec VS message size
# 16 CPU threads
# bytes per message = [64, 256, 1K, 4K, 8K]
# 1 message per connection

top(i) = 0.98 - 0.93 * i / 3
bot(i) = top(i + 1) + 0.03

set terminal pngcairo dashed size 1680,1050
set style data linespoints
set output outfile
set multiplot layout 3, 3
set lmargin 7

set grid y
unset key
fig1(infile) = "< awk -e '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
fig2(infile) = "< awk -e '//{if ($1==16&&$2==64)print $0}' ".infile.'| sort -nk3'
fig3(infile) = "< awk -e '//{if ($1==16&&$3==1)print $0 }' ".infile.'| sort -nk2'

set tmargin at screen top(0);set bmargin at screen bot(0)
set ylabel 'Messages/sec (x 10^6)' offset 1
set yrange [0:5]
plot for [f in infile] fig1(f) using ($6/10**6):xticlabel(1)
unset ylabel
set yrange [0:16]
plot for [f in infile] fig2(f) using ($6/10**6):xticlabel(3)
set yrange [0:40]
set ylabel 'Throughput (Gbps)' offset 1
plot for [f in infile] fig3(f) using ($2*$6*8/10**9):xticlabel(2)
set tmargin at screen top(1);set bmargin at screen bot(1)
set yrange [0.01:100]
set logscale y
set ylabel '99% latency (ms)' offset 2
plot for [f in infile] fig1(f) using ($12/1000):xticlabel(1)
unset ylabel
plot for [f in infile] fig2(f) using ($12/1000):xticlabel(3)
plot for [f in infile] fig3(f) using ($12/1000):xticlabel(2)
set tmargin at screen top(2);set bmargin at screen bot(2)
set nologscale y
set ylabel 'Average power (W)' offset 1
set xlabel 'Number of CPU threads'
set yrange [0:120]
plot for [i=1:words(infile)] fig1(word(infile,i)) using (column(14+word(cpu,i))/60):xticlabel(1)
unset ylabel
set xlabel 'Number of Messages per Connection'
plot for [i=1:words(infile)] fig2(word(infile,i)) using (column(14+word(cpu,i))/60):xticlabel(3)
set xlabel 'Message Size'
plot for [i=1:words(infile)] fig3(word(infile,i)) using (column(14+word(cpu,i))/60):xticlabel(2)

unset origin
unset border
unset tics
unset label
unset arrow
unset title
unset object
set tmargin at screen 0.03;set bmargin at screen 0
set size 1
set key box horizontal reverse center top
set yrange [-1:1]
plot for [i=1:words(infile)] NaN title word(title,i)
