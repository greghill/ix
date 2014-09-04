#!/bin/bash

nth() {
  ls $2/results/*/$3|sort -r|sed -n "$1{p;q;}"
}

if [ $1. = 'published.' ]; then
  short_ix=papers/osdi14/figs/data/short/IX-10-RPC/Linux-Libevent/data
  short_mtcp=papers/osdi14/figs/data/short/mTCP-10-RPC/Linux-Libevent/data
  short_linux=papers/osdi14/figs/data/short/Linux-10-RPC/Linux-Libevent/data
  short_ix40=papers/osdi14/figs/data/short/IX-40-RPC/Linux-Libevent/data
  connscaling_ix40=papers/osdi14/figs/data/connscaling/IX-40-RPC/Linux-Libevent/data
  connscaling_linux40=papers/osdi14/figs/data/connscaling/Linux-40-RPC/Linux-Libevent/data
  netpipe_ix=papers/osdi14/figs/data/pingpong/IX-10-RPC/IX/data
  netpipe_mtcp=papers/osdi14/figs/data/pingpong/Netpipe-mTCP/Netpipe-mTCP/data
  netpipe_linux=papers/osdi14/figs/data/pingpong/Netpipe-10/Netpipe/data
  memcached_ix_etc=papers/osdi14/figs/data/memcached/IX-10/fb_etc.csv
  memcached_ix_usr=papers/osdi14/figs/data/memcached/IX-10/fb_usr.csv
  memcached_linux_etc=papers/osdi14/figs/data/memcached/Linux-10/fb_etc.csv
  memcached_linux_usr=papers/osdi14/figs/data/memcached/Linux-10/fb_usr.csv
else
  short_ix=`nth 1 bench short/IX-10-RPC/Linux-Libevent/data`
  short_mtcp=`nth 1 bench short/mTCP-10-RPC/Linux-Libevent/data`
  short_linux=`nth 1 bench short/Linux-10-RPC/Linux-Libevent/data`
  short_ix40=`nth 1 bench short/IX-40-RPC/Linux-Libevent/data`
  connscaling_ix40=`nth 1 bench short/IX-40-RPC/Linux-Libevent/data`
  connscaling_linux40=`nth 1 bench short/Linux-40-RPC/Linux-Libevent/data`
  netpipe_ix=`nth 1 bench pingpong/IX-10-RPC/IX/data`
  netpipe_mtcp=`nth 1 bench pingpong/Netpipe-mTCP/Netpipe-mTCP/data`
  netpipe_linux=`nth 1 bench pingpong/Netpipe-10/Netpipe/data`
  memcached_ix_etc=`nth 1 experiments IX-10/fb_etc.csv`
  memcached_ix_usr=`nth 1 experiments IX-10/fb_usr.csv`
  memcached_linux_etc=`nth 1 experiments Linux-10/fb_etc.csv`
  memcached_linux_usr=`nth 1 experiments Linux-10/fb_usr.csv`
fi

echo Using files:
echo $short_ix
echo $short_mtcp
echo $short_linux
echo $short_ix40
echo $connscaling_ix40
echo $connscaling_linux40
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
echo
echo evaluation
echo ----------
awk '//{if ($3==1024) printf "for 1024 roundtrips, IX delivers %.1f million messages per second\n", $4/10**6}' $short_ix
paste <(awk '//{if ($3==1024)print $4}' $short_ix) <(awk '//{if ($3==1024)print $4}' $short_mtcp) <(awk '//{if ($3==1024)print $4}' $short_linux) | awk '//{printf "which is %.1fx the throughput of mTCP and of and %.1fx that of Linux\n", $1/$2, $1/$3}'
awk '//{if ($1==16&&$2==64&&$3==1) printf "IX linearly scales to deliver %.1f million TCP connections per second on 4x10GbE\n", $4/10**6}' $short_ix40
paste $short_ix $short_ix40 | awk '//{if ($1==16&&$2==64&&$3==1) printf "speedup of %d%% with n = 1\n", 100*($13-$4)/$4}'
paste $short_ix $short_ix40 | awk '//{if ($1==16&&$2==64&&$3==1024) printf "and of %d%% with n = 1K over the 10GbE configuration\n", 100*($13-$4)/$4}'
awk '//{if ($1==16&&$2==8192&&$3==1) printf "IX can deliver 8KB messages with a goodput of %.1f Gbps\n", $4*$2*8/10**9}' $short_ix40
awk '//{if ($1==16&&$2==8192&&$3==1) printf "for a wire throughput of %.1f Gpbs\n", $7*8/10**9}' $short_ix40
paste $connscaling_ix40 $connscaling_linux40 | awk '//{if ($4<$ix)next;ix=$4;linux=$13}END{printf "IX performs %dx better than Linux\n", ix/linux}'
awk '//{maxconn=$4;if ($4<peak)next;peak=$4}END{printf "IX is able to deliver %d%% of its own peak throughput\n", 100*maxconn/peak}' $connscaling_ix40
