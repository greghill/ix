#!/bin/bash

source bench_init.sh

run() {
  cores=$1
  msg_size=$2
  msg_per_conn=$3

  echo -ne "$[1+$(echo $cores|grep -o ','|wc -l)]\t$msg_size\t$msg_per_conn\t"
  ./launch.py --conf $CFG_FILE --with-server --clients 18 --client-params "16 50 $msg_size $msg_per_conn" --server-params "$(eval echo $SERVER_PARAMS)"
  sleep 5
}

common() {
  run $CORES 64 1
}

fig1() {
  count=$(echo $CORES|grep -o ','|wc -l)
  for i in $(seq 1 $count); do
    # TODO: maybe we need to balance here the IRQs for the Linux case
    run $(echo $CORES|cut -d',' -f-$i) 64 1
  done
}

fig2() {
  for i in 2 8 32 64 128; do
    run $CORES 64 $i
  done
}

fig3() {
  for i in 256 1024 4096 8192; do
    run $CORES $i 1
  done
}

dir=$(bench_start "mtcp")
common > $dir/data
fig1 >> $dir/data
fig2 >> $dir/data
fig3 >> $dir/data
bench_stop $dir
