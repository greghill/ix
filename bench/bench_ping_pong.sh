#!/bin/bash

source bench_init.sh

run() {
  cores=$1
  msg_size=$2

  echo -ne "$msg_size\t"
  ./launch.py --conf $CFG_FILE --with-server --clients 1 --client-params "16 1 $msg_size 999999999999" --server-params "$(eval echo $SERVER_PARAMS)"
  sleep 5
}

fig() {
  for i in 64 128 256 512 1024 2048 3072 4096 6144 8192 12288 16384; do
    run $CORES $i
  done
}

dir=$(bench_start "ping_pong")
fig > $dir/data
bench_stop $dir
