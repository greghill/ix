#!/bin/bash

source bench_init.sh

run() {
  cores=$1
  conn_per_client=$2

  ./launch.py --conf $CFG_FILE --with-server --clients 18 --client-params "16 $conn_per_client 64 1" --server-params "$(eval echo $SERVER_PARAMS)"
  sleep 5
}

fig() {
  for i in 1 2 5 10 20 50; do
    run $CORES $i
  done
}

dir=$(bench_start "power_vs_throughput")
fig > $dir/data
bench_stop $dir
