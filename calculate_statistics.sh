#!/bin/bash

nth() {
  ls $2/results/*/$3|sort -r|sed -n "$1{p;q;}"
}

if [ $1. = 'published.' ]; then
  netpipe_ix=papers/osdi14/figs/data/pingpong/IX-10-RPC/IX/data
  netpipe_mtcp=papers/osdi14/figs/data/pingpong/Netpipe-mTCP/Netpipe-mTCP/data
  netpipe_linux=papers/osdi14/figs/data/pingpong/Netpipe-10/Netpipe/data
  memcached_ix_etc=papers/osdi14/figs/data/memcached/IX-10/fb_etc.csv
  memcached_ix_usr=papers/osdi14/figs/data/memcached/IX-10/fb_usr.csv
  memcached_linux_etc=papers/osdi14/figs/data/memcached/Linux-10/fb_etc.csv
  memcached_linux_usr=papers/osdi14/figs/data/memcached/Linux-10/fb_usr.csv
else
  netpipe_ix=`nth 1 bench pingpong/IX-10-RPC/IX/data`
  netpipe_mtcp=`nth 1 bench pingpong/Netpipe-mTCP/Netpipe-mTCP/data`
  netpipe_linux=`nth 1 bench pingpong/Netpipe-10/Netpipe/data`
  memcached_ix_etc=`nth 1 experiments IX-10/fb_etc.csv`
  memcached_ix_usr=`nth 1 experiments IX-10/fb_usr.csv`
  memcached_linux_etc=`nth 1 experiments Linux-10/fb_etc.csv`
  memcached_linux_usr=`nth 1 experiments Linux-10/fb_usr.csv`
fi

echo Using files:
echo $netpipe_ix
echo $netpipe_mtcp
echo $netpipe_linux
echo $memcached_ix_etc
echo $memcached_ix_usr
echo $memcached_linux_etc
echo $memcached_linux_usr
echo
echo NetPIPE one-way latencies
echo -------------------------
head -1 $netpipe_ix    | awk '//{printf "IX    <-> IX   : %5.1f μs\n", 1000000/$4/2}'
head -1 $netpipe_mtcp  | awk '//{printf "mTCP  <-> mTCP : %5.1f μs\n", 1000000/$4/2}'
head -1 $netpipe_linux | awk '//{printf "Linux <-> Linux: %5.1f μs\n", 1000000/$4/2}'
echo
echo memcached
echo ---------
awk 'BEGIN{FS=",";m=9999;n=9999}//{m=$10<m?$10:m;if ($10<500)n=$11>n?$11:n}END{printf "ETC-Linux: min latency@99th: %5.1f μs QPS for SLA: %5.1f KQPS\n", m, n/1000}' $memcached_linux_etc
awk 'BEGIN{FS=",";m=9999;n=9999}//{m=$10<m?$10:m;if ($10<500)n=$11>n?$11:n}END{printf "ETC-IX:    min latency@99th: %5.1f μs QPS for SLA: %5.1f KQPS\n", m, n/1000}' $memcached_ix_etc
awk 'BEGIN{FS=",";m=9999;n=9999}//{m=$10<m?$10:m;if ($10<500)n=$11>n?$11:n}END{printf "USR-Linux: min latency@99th: %5.1f μs QPS for SLA: %5.1f KQPS\n", m, n/1000}' $memcached_linux_usr
awk 'BEGIN{FS=",";m=9999;n=9999}//{m=$10<m?$10:m;if ($10<500)n=$11>n?$11:n}END{printf "USR-IX:    min latency@99th: %5.1f μs QPS for SLA: %5.1f KQPS\n", m, n/1000}' $memcached_ix_usr
