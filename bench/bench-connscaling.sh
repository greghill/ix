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
  CLIENTS="$CLIENTS icnals1|eth1"
  CLIENTS="$CLIENTS icnals2|eth1"
  CLIENTS="$CLIENTS icnals3|eth1"
  CLIENTS="$CLIENTS icnals4|eth1"
  CLIENTS="$CLIENTS icnals5|eth1"
  CLIENTS="$CLIENTS icnals6|eth1"
  CLIENTS="$CLIENTS icnals7|eth1"
  CLIENTS="$CLIENTS icnals8|eth1"
  CLIENTS="$CLIENTS icnals9|eth1"
  CLIENTS="$CLIENTS icnals10|eth1"
  CLIENTS="$CLIENTS icnals11|eth2"
  CLIENTS="$CLIENTS icnals12|eth2"
  CLIENTS="$CLIENTS icnals13|eth2"
  CLIENTS="$CLIENTS icnals14|eth2"
  CLIENTS="$CLIENTS icnals15|eth2"
  CLIENTS="$CLIENTS icnals16|eth2"
  CLIENTS="$CLIENTS icnals17|eth2"
  CLIENTS="$CLIENTS icnals18|eth2"
  SERVER_IP=192.168.21.1
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
elif [ $CLUSTER_ID = 'Stanford-IX' ]; then
  CLIENTS="$CLIENTS maverick-1|p3p1|10.79.6.11"	
  CLIENTS="$CLIENTS maverick-2|p3p1|10.79.6.13"
  CLIENTS="$CLIENTS maverick-4|p3p1|10.79.6.15"
  CLIENTS="$CLIENTS maverick-7|p3p1|10.79.6.18"
  CLIENTS="$CLIENTS maverick-8|p3p1|10.79.6.19"
  CLIENTS="$CLIENTS maverick-10|p7p1|10.79.6.21"
  CLIENTS="$CLIENTS maverick-12|p3p1|10.79.6.23"
  CLIENTS="$CLIENTS maverick-14|p3p1|10.79.6.25"
  CLIENTS="$CLIENTS maverick-16|p3p1|10.79.6.27"
  CLIENTS="$CLIENTS maverick-18|p3p1|10.79.6.29"
  SERVER_IP=10.79.6.112
else
  echo 'invalid parameters' >&2
  exit 1
fi

TIME=10

if [ $# -lt 2 ]; then
  echo "Usage: $0 SERVER_SPEC CLIENT_SPEC [CLUSTER_ID]"
  echo "  SERVER_SPEC = IX-10-RPC|IX-10-Stream|IX-40-RPC|IX-40-Stream"
  echo "              | Linux-10-RPC|Linux-10-Stream|Linux-40-RPC|Linux-40-Stream"
  echo "  CLIENT_SPEC = Linux-Libevent | Linux-Simple"
  echo "  CLUSTER_ID  = EPFL|Stanford (default: EPFL)"
  exit
fi

SERVER_SPEC=$1
CLIENT_SPEC=$2

if [ -z $SERVER_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $SERVER_SPEC = 'IX-10-RPC' ]; then
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  BUILD_IX=1
  BUILD_TARGET_BENCH=
  if [ $CLUSTER_ID = 'EPFL' ]; then
    SERVER_NET="ix node1"
    CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
    IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
  elif [ $CLUSTER_ID = 'Stanford-IX' ]; then
    SERVER_NET="ix node0"
    CORES="0,1,2,3,4,5"
    IX_PARAMS="-d 0000:04:00.0 -c \$IX_PARAMS_CORES"
  fi
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
elif [ $SERVER_SPEC = 'Linux-10-Stream' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=0
  BUILD_TARGET_BENCH=
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
  CLIENT_CMDLINE="./client $SERVER_IP $SERVER_PORT 16 \$CLIENT_CONNECTIONS 64 999999999"
  DEPLOY_FILES="select_net.sh client"
  CLIENT_NET="linux single"
elif [ $CLIENT_SPEC = 'Linux-Simple' ]; then
  CLIENT_CMDLINE="./simple_client $SERVER_IP $SERVER_PORT \$THREADS \$CONNECTIONS_PER_THREAD 64 999999999 1 0"
  DEPLOY_FILES="select_net.sh simple_client"
  CLIENT_NET="linux single"
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

on_exit_linux_stream() {
  PID=`pidof server||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

server_ix_rpc() {
  IX_PARAMS_CORES=$CORES
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/event/pingpong 64 65536 >> ix.log 2>&1) &
}

server_ix_stream() {
  IX_PARAMS_CORES=$CORES
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/event/stream 65536 >> ix.log 2>&1) &
}

server_linux_rpc() {
  $DIR/pingpongs 64 $CORES &
}

server_linux_stream() {
  $DIR/server $CORES &
}

run_single() {
  CONNECTIONS=$1

  THREADS=$[$CONNECTIONS / $CLIENT_COUNT]
  if [ $THREADS -gt 24 ]; then
    THREADS=24
  fi
  CONNECTIONS_PER_THREAD=$[$CONNECTIONS / $CLIENT_COUNT / $THREADS]
  CLIENT_CONNECTIONS=$[$CONNECTIONS_PER_THREAD * $THREADS]
  CONNECTIONS=$[$CONNECTIONS_PER_THREAD * $CLIENT_COUNT * $THREADS]
  $SERVER
  ssh $HOST "while ! nc -w 1 $SERVER_IP $SERVER_PORT; do sleep 1; i=\$[i+1]; if [ \$i -eq 30 ]; then exit 1; fi; done"
  echo -ne "$CONNECTIONS\t64\t999999999\t" >> $OUTDIR/data
  python $DIR/launch.py --time $TIME --clients $CLIENT_HOSTS --client-cmdline "`eval echo $CLIENT_CMDLINE`" >> $OUTDIR/data
  $ON_EXIT
}

run() {
  for i in `awk "BEGIN{for(i=1.4;i<=5.1;i+=0.2)printf \"%d \",10**i}"`; do
    run_single $i
  done
}

prepare $BUILD_IX $BUILD_TARGET_BENCH
OUTDIR=`bench_start "connscaling/$SERVER_SPEC/$CLIENT_SPEC"`
trap $ON_EXIT EXIT
run
bench_stop $OUTDIR
