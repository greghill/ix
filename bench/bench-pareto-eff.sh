#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[1]}: line ${BASH_LINENO[0]}: Failed at `date`"
}
trap on_err ERR

DIR=`dirname $0`
CLIENTS=`for i in {1..18}; do echo -n " "icnals$i; done`
CLIENT_NET="linux single"
DEPLOY_FILES="$DIR/../apps/mutilate/mutilate $DIR/select_net.sh $DIR/power-wrapper.py"
FIRST_CLIENT=`echo $CLIENTS|awk '{print $1}'`
REST_CLIENTS=`echo $CLIENTS|awk '{$1="";print}'`
SERVER_IP=192.168.21.1
CORE_LIST=1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31
#CORE_LIST=1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31

if [ $1 = Linux ]; then
  SERVER_NET="linux single"
  SERVER_PORT=11211
  server() {
    CORES=$1
    LIST=$2
    ( trap ERR; $DIR/../apps/memcached-1.4.18/memcached -c 2048 -t $CORES -T $LIST ) &
  }
elif [ $1 = IX ]; then
  SERVER_NET="ix node1"
  SERVER_PORT=8000
  server() {
    CORES=$1
    LIST=$2
    IX_PARAMS="-d 0000:42:00.1 -c $LIST"
    ( trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix $IX_PARAMS -- /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/memcached-1.4.18-ix/memcached >> ~/ix.log 2>&1 ) &
  }
fi

MUTILATE_PARAMS="--records=1000000 --binary --keysize=19 --valuesize=2 --server=$SERVER_IP:$SERVER_PORT"
MUTILATE_LOAD_PARAMS="$MUTILATE_PARAMS --loadonly"
MUTILATE_BENCHMARK_PARAMS="$MUTILATE_PARAMS --qps-function=triangle:10000:3500000:120 --time=60 --report-stats=1 --noload --threads=1 --connections=4 --depth=4 --measure_connections=1 --measure_depth=1 --measure_qps=2000 --update=0.002 --agent=`echo $REST_CLIENTS|tr ' ' ','`"

. $DIR/bench-common.sh

on_exit() {
  for i in $CLIENTS; do ( ssh $i killall -INT mutilate 2>/dev/null & ) ; done
  sudo killall -KILL ix 2>/dev/null || true
  killall memcached 2>/dev/null || true
  pgrep -f rapl-server.py|xargs sudo kill 2>/dev/null || true
}

wait_alive() {
  HOST=$1
  SERVER_IP=$2
  SERVER_PORT=$3
  TIMEOUT=$4
  if ssh $HOST "while ! nc -w 1 $SERVER_IP $SERVER_PORT; do sleep 1; i=\$[i+1]; if [ \$i -eq $TIMEOUT ]; then exit 1; fi; done"; then
    return 1
  fi
  return 0
}

set_freq() {
  PACKAGE=$1
  FREQ=$2
  sudo sh <<EOF
  for i in /sys/devices/system/cpu/cpu[0-9]*; do
    if [ \`cat \$i/topology/physical_package_id\` -eq $PACKAGE ]; then
      echo $FREQ > \$i/cpufreq/scaling_setspeed
    fi
  done
EOF
}

prepare

MINFREQ=`awk '{ print $NF }' /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
set_freq 0 $MINFREQ

trap on_exit EXIT
OUTDIR=`bench_start "memcached-pareto/$1"`
for CORES in {1..16}; do
  LIST=`cut -d, -f-$CORES <<<$CORE_LIST`
  MASK=`python -c 'print "%08x"%reduce(lambda x,y:x|1<<int(y),"'"$LIST"'".split(","),0)'`
  sudo $DIR/rapl-server.py &
  server $CORES $LIST
  while wait_alive $FIRST_CLIENT $SERVER_IP $SERVER_PORT 30; do
    on_exit
    server $CORES $LIST
  done
  ssh $FIRST_CLIENT "killall mutilate 2>/dev/null;./mutilate $MUTILATE_LOAD_PARAMS"
  for FREQ in `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`; do
    set_freq 1 $FREQ
    for i in $REST_CLIENTS; do ( ssh $i 'killall -INT mutilate 2>/dev/null;./mutilate --agentmode --threads=16' & ) ; done
    ssh $FIRST_CLIENT "killall mutilate 2>/dev/null;./power-wrapper.py --line sciicebpc1:4242 ./mutilate $MUTILATE_BENCHMARK_PARAMS" | sed "s/^/$CORES $MASK $FREQ /" >> $OUTDIR/mutilate.out
  done
  on_exit
done
bench_stop $OUTDIR
