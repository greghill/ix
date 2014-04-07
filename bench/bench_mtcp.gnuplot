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

set terminal pngcairo dashed size 1680,1050
set style data linespoints
set output outfile
set multiplot layout 3, 3
set lmargin 7
set grid y
unset key
fig1 = "< awk -e '//{if ($2==64&&$3==1)print $0 }' ".infile.'| sort -nk1'
fig2 = "< awk -e '//{if ($1==16&&$2==64)print $0}' ".infile.'| sort -nk3'
fig3 = "< awk -e '//{if ($1==16&&$3==1)print $0 }' ".infile.'| sort -nk2'
set ylabel 'Messages/sec (x 10^6)' offset 1
set yrange [0:*]
plot fig1 using ($6/10**6):xticlabel(1)
unset ylabel
plot fig2 using ($6/10**6):xticlabel(3)
plot fig3 using ($6/10**6):xticlabel(2)
set yrange [*:*]
set logscale y
set ylabel '99% latency (ms)' offset 2
plot fig1 using ($12/1000):xticlabel(1)
unset ylabel
plot fig2 using ($12/1000):xticlabel(3)
plot fig3 using ($12/1000):xticlabel(2)
set nologscale y
set ylabel 'Average power (W)' offset 1
set xlabel 'Number of CPU threads'
set yrange [0:*]
plot fig1 using (column(14+cpu)/60):xticlabel(1)
unset ylabel
set xlabel 'Number of Messages per Connection'
plot fig2 using (column(14+cpu)/60):xticlabel(3)
set xlabel 'Message Size'
plot fig3 using (column(14+cpu)/60):xticlabel(2)
