#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[1]}: line ${BASH_LINENO[0]}: Failed at `date`"
}
trap on_err ERR

### conf

if [ $# -ge 3 ]; then
  CLUSTER_ID=$3
else
  CLUSTER_ID="EPFL"
fi

CLIENTS=

if [ $CLUSTER_ID = 'EPFL' ]; then
  CLIENTS="$CLIENTS icnals1|eth1|192.168.21.11"
  CLIENTS="$CLIENTS icnals2|eth1|192.168.21.12"
  CLIENTS="$CLIENTS icnals3|eth1|192.168.21.13"
  CLIENTS="$CLIENTS icnals4|eth1|192.168.21.14"
  CLIENTS="$CLIENTS icnals5|eth1|192.168.21.15"
  CLIENTS="$CLIENTS icnals6|eth1|192.168.21.16"
  CLIENTS="$CLIENTS icnals7|eth1|192.168.21.17"
  CLIENTS="$CLIENTS icnals8|eth1|192.168.21.18"
  CLIENTS="$CLIENTS icnals9|eth1|192.168.21.19"
  CLIENTS="$CLIENTS icnals10|eth1|192.168.21.20"
  CLIENTS="$CLIENTS icnals11|eth2|192.168.21.21"
  CLIENTS="$CLIENTS icnals12|eth2|192.168.21.22"
  CLIENTS="$CLIENTS icnals13|eth2|192.168.21.23"
  CLIENTS="$CLIENTS icnals14|eth2|192.168.21.24"
  CLIENTS="$CLIENTS icnals15|eth2|192.168.21.25"
  CLIENTS="$CLIENTS icnals16|eth2|192.168.21.26"
  CLIENTS="$CLIENTS icnals17|eth2|192.168.21.27"
  CLIENTS="$CLIENTS icnals18|eth2|192.168.21.28"
  SERVER_IP=192.168.21.1
  CLIENT_CORES=16
  CLIENT_CONNECTIONS=50
elif [ $CLUSTER_ID = 'Stanford' ]; then
  CLIENTS="$CLIENTS maverick-1|p3p1|10.79.6.11"
  CLIENTS="$CLIENTS maverick-2|p3p1|10.79.6.13"
  CLIENTS="$CLIENTS maverick-4|p3p1|10.79.6.15"
  CLIENTS="$CLIENTS maverick-7|p3p1|10.79.6.18"
  CLIENTS="$CLIENTS maverick-8|p3p1|10.79.6.19"
  CLIENTS="$CLIENTS maverick-10|p7p1|10.79.6.21"
  CLIENTS="$CLIENTS maverick-12|p3p1|10.79.6.23"
  CLIENTS="$CLIENTS maverick-14|p3p1|10.79.6.25"
  #CLIENTS="$CLIENTS maverick-16|p3p1|10.79.6.27"
  #CLIENTS="$CLIENTS maverick-18|p3p1|10.79.6.29"
  SERVER_IP=10.79.6.22
  CLIENT_CORES=24
  CLIENT_CONNECTIONS=96
else
  echo 'invalid parameters' >&2
  exit 1
fi

TIME=10

if [ $# -lt 2 ]; then
  echo "Usage: $0 SERVER_SPEC CLIENT_SPEC [CLUSTER_ID]"
  echo "  SERVER_SPEC = IX-10-RPC|IX-10-Stream|IX-40-RPC|IX-40-Stream"
  echo "              | Linux-10-RPC|Linux-10-Stream|Linux-40-RPC|Linux-40-Stream"
  echo "              | mTCP-10-RPC|mTCP-10-Stream"
  echo "  CLIENT_SPEC = Linux-Libevent|Linux-Simple"
  echo "  CLUSTER_ID  = EPFL|Stanford (default: EPFL)"
  exit
fi

SERVER_SPEC=$1
CLIENT_SPEC=$2

if [ -z $SERVER_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $SERVER_SPEC = 'IX-10-RPC' ]; then
  SERVER_NET="ix node1"
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=1
  BUILD_TARGET_BENCH=
  IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'IX-40-RPC' ]; then
  SERVER_NET="ix node0"
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=1
  BUILD_TARGET_BENCH=
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'Linux-10-RPC' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=0
  BUILD_TARGET_BENCH=
elif [ $SERVER_SPEC = 'mTCP-10-RPC' ]; then
  SERVER_NET="mtcp"
  SERVER=server_mtcp_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_mtcp_rpc
  CORES="0,1,2,3,4,5"
  BUILD_IX=0
  BUILD_TARGET_BENCH="all_mtcp"
elif [ $SERVER_SPEC = 'Linux-10-Stream' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=0
  BUILD_TARGET_BENCH=
elif [ $SERVER_SPEC = 'mTCP-10-Stream' ]; then
  SERVER_NET="mtcp"
  SERVER=server_mtcp_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_mtcp_stream
  CORES="0,1,2,3,4,5"
  BUILD_IX=0
  BUILD_TARGET_BENCH="all_mtcp"
elif [ $SERVER_SPEC = 'Linux-40-RPC' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=0
  BUILD_TARGET_BENCH=
elif [ $SERVER_SPEC = 'Linux-40-Stream' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=0
  BUILD_TARGET_BENCH=
elif [ $SERVER_SPEC = 'IX-10-Stream' ]; then
  SERVER_NET="ix node1"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=1
  BUILD_TARGET_BENCH=
  IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'IX-40-Stream' ]; then
  SERVER_NET="ix node0"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=1
  BUILD_TARGET_BENCH=
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
else
  echo 'invalid parameters' >&2
  exit 1
fi

if [ -z $CLIENT_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $CLIENT_SPEC = 'Linux-Libevent' ]; then
  CLIENT_CMDLINE="./client $SERVER_IP $SERVER_PORT $CLIENT_CORES $CLIENT_CONNECTIONS \$MSG_SIZE \$MSG_PER_CONN"
  DEPLOY_FILES="select_net.sh client"
  CLIENT_NET="linux single"
elif [ $CLIENT_SPEC = 'Linux-Simple' ]; then
  THREADS=24
  CONNECTIONS_PER_THREAD=2
  CLIENT_CMDLINE="./simple_client $SERVER_IP $SERVER_PORT $THREADS $CONNECTIONS_PER_THREAD \$MSG_SIZE \$MSG_PER_CONN 0 0"
  DEPLOY_FILES="select_net.sh simple_client"
  CLIENT_NET="linux single"
elif [ $CLIENT_SPEC = 'IX' ]; then
  echo 'not implemented' >&2
  exit 1
else
  echo 'invalid parameters' >&2
  exit 1
fi

DIR=`dirname $0`

. $DIR/bench-common.sh

on_exit_ix() {
  PID=`pidof ix||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  sudo kill -KILL $PID
}

on_exit_linux_rpc() {
  PID=`pidof pingpongs||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

on_exit_mtcp_rpc() {
  PID=`pidof pingpongs_mtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
  rm -f log_*
}

on_exit_linux_stream() {
  PID=`pidof server||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

on_exit_mtcp_stream() {
  PID=`pidof server_mtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
  rm -f log_*
}

server_ix_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  IX_PARAMS_CORES=`echo $CORES|cut -d',' -f-$CORE_COUNT`
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/event/pingpong $MSG_SIZE >> ix.log 2>&1) &
}

server_ix_stream() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  IX_PARAMS_CORES=`echo $CORES|cut -d',' -f-$CORE_COUNT`
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/event/stream >> ix.log 2>&1) &
}

server_linux_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/pingpongs $MSG_SIZE `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

server_mtcp_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/pingpongs_mtcp $MSG_SIZE `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

server_linux_stream() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/server `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

server_mtcp_stream() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/server_mtcp `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

run_single() {
  CORE_COUNT=$1
  MSG_SIZE=$2
  MSG_PER_CONN=$3

  $SERVER $CORE_COUNT $MSG_SIZE
  ssh $HOST "while ! nc -w 1 $SERVER_IP $SERVER_PORT; do sleep 1; i=\$[i+1]; if [ \$i -eq 30 ]; then exit 1; fi; done"
  echo -ne "$CORE_COUNT\t$MSG_SIZE\t$MSG_PER_CONN\t" >> $OUTDIR/data
  python $DIR/launch.py --time $TIME --clients $CLIENT_HOSTS --client-cmdline "`eval echo $CLIENT_CMDLINE`" >> $OUTDIR/data
  $ON_EXIT
}

run() {
  run_single 16 64 1
  for i in {1..15}; do
    run_single $i 64 1
  done
  for i in 2 8 32 64 128 256 512 1024; do
    run_single 16 64 $i
  done
  for i in 256 1024 4096 8192; do
    run_single 16 $i 1
  done
}

prepare $BUILD_IX $BUILD_TARGET_BENCH
OUTDIR=`bench_start "short/$SERVER_SPEC/$CLIENT_SPEC"`
trap $ON_EXIT EXIT
run
bench_stop $OUTDIR
