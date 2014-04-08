#!/bin/bash

set -e

CPU_FREQ=2.4G
CPU0_CORES=0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30
CPU1_CORES=1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31
SINGLE_NIC=0000:42:00.1
BOND_NICS=0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1

CFG=$1
if [ -z $CFG ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $CFG = 'ix-1nic' ]; then
  NET=ix-numa1
  CORES=$CPU1_CORES
  SERVER_PARAMS="-d $SINGLE_NIC -c \$cores"
  CFG_FILE=cfg-ix.py
elif [ $CFG = 'ix-4nic' ]; then
  NET=ix-numa0
  CORES=$CPU0_CORES
  SERVER_PARAMS="-d $BOND_NICS -c \$cores"
  CFG_FILE=cfg-ix.py
elif [ $CFG = 'linux-1nic' ]; then
  NET=single
  CORES=$CPU1_CORES
  SERVER_PARAMS="\$cores"
  CFG_FILE=cfg-linux.py
elif [ $CFG = 'linux-4nic' ]; then
  NET=bond
  CORES=$CPU0_CORES
  SERVER_PARAMS="\$cores"
  CFG_FILE=cfg-linux.py
else
  echo 'invalid parameter' >&2
  exit 1
fi

for i in $(seq 0 $[ $(grep -c processor /proc/cpuinfo) - 1 ]); do sudo cpufreq-set -c $i -f $CPU_FREQ; done
sudo ./select_net.sh $NET

make clean > /dev/null
make > /dev/null
( cd ../ix && make clean > /dev/null && make -j64 > /dev/null )

./launch.py --conf cfg-ix.py --clients 18 --with-server --deploy

set +e

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

dir=results/$(date +%Y-%m-%d-%H-%M-%S)/$CFG
mkdir -p $dir
sha1=$(git rev-parse HEAD)
if ! git diff-index --quiet HEAD; then
  sha1="$sha1+"
fi
echo "SHA1: $sha1" > $dir/info
echo "Start: $(date)" >> $dir/info
echo "--- git diff begin ---" >> $dir/info
git diff HEAD >> $dir/info
echo "--- git diff end ---" >> $dir/info
common > $dir/data
fig1 >> $dir/data
fig2 >> $dir/data
fig3 >> $dir/data
echo "Stop: $(date)" >> $dir/info
echo "Done" >> $dir/info
