#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[0]}: line ${BASH_LINENO[0]}: Failed at `date`"
}
trap on_err ERR

### conf

CLIENTS="icnals1|eth1"
SERVER_IP=192.168.21.1
TIME=10

if [ $# -lt 2 ]; then
  echo "Usage: $0 SERVER_SPEC CLIENT_SPEC"
  echo "  SERVER_SPEC = IX-10-RPC|IX-10-Stream|IX-40-RPC|IX-40-Stream"
  echo "              | Linux-10-RPC|Linux-10-Stream|Linux-40-RPC|Linux-40-Stream"
  echo "              | Netpipe-10 | Netpipe-40"
  echo "              | Netpipe-10-Optimized | Netpipe-40-Optimized"
  echo "  CLIENT_SPEC = Linux-Libevent|Linux-Simple|Netpipe"
  echo "              | Netpipe-Optimized"
  exit
fi

SERVER_SPEC=$1
CLIENT_SPEC=$2

NETPIPE=0

if [ -z $SERVER_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $SERVER_SPEC = 'IX-10-RPC' ]; then
  SERVER_NET="ix node1"
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'IX-40-RPC' ]; then
  SERVER_NET="ix node0"
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'Linux-10-RPC' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
elif [ $SERVER_SPEC = 'Linux-10-Stream' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
elif [ $SERVER_SPEC = 'Linux-40-RPC' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
elif [ $SERVER_SPEC = 'Linux-40-Stream' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
elif [ $SERVER_SPEC = 'IX-10-Stream' ]; then
  SERVER_NET="ix node1"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'IX-40-Stream' ]; then
  SERVER_NET="ix node0"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'Netpipe-10' ]; then
  SERVER_NET="linux single"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
elif [ $SERVER_SPEC = 'Netpipe-40' ]; then
  SERVER_NET="linux bond"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
elif [ $SERVER_SPEC = 'Netpipe-10-Optimized' ]; then
  SERVER_NET="linux single opt"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
elif [ $SERVER_SPEC = 'Netpipe-40-Optimized' ]; then
  SERVER_NET="linux bond opt"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
else
  echo 'invalid parameters' >&2
  exit 1
fi

if [ -z $CLIENT_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $CLIENT_SPEC = 'Linux-Libevent' ]; then
  CLIENT_CMDLINE="./client $SERVER_IP $SERVER_PORT 1 1 \$MSG_SIZE 999999999"
  DEPLOY_FILES="select_net.sh client"
  CLIENT_NET="linux single"
elif [ $CLIENT_SPEC = 'Linux-Simple' ]; then
  THREADS=1
  CONNECTIONS_PER_THREAD=1
  CLIENT_CMDLINE="./simple_client $SERVER_IP $SERVER_PORT $THREADS $CONNECTIONS_PER_THREAD \$MSG_SIZE 999999999"
  DEPLOY_FILES="select_net.sh simple_client"
  CLIENT_NET="linux single"
elif [ $CLIENT_SPEC = 'Netpipe' ]; then
  DEPLOY_FILES="select_net.sh NPtcp"
  CLIENT_NET="linux single"
  NETPIPE=$[$NETPIPE+1]
elif [ $CLIENT_SPEC = 'Netpipe-Optimized' ]; then
  DEPLOY_FILES="select_net.sh NPtcp"
  CLIENT_NET="linux single opt"
  NETPIPE=$[$NETPIPE+1]
elif [ $CLIENT_SPEC = 'IX' ]; then
  echo 'not implemented' >&2
  exit 1
else
  echo 'invalid parameters' >&2
  exit 1
fi

DIR=`dirname $0`

if [ $NETPIPE -eq 1 ]; then
  echo 'Netpipe must be selected on both server and client' 2>&1
  exit 1
fi

. $DIR/bench-common.sh

on_exit_ix() {
  sudo kill -KILL `pidof ix` 2>/dev/null
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

on_exit_netpipe() {
  PID=`pidof NPtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

server_ix_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  IX_PARAMS_CORES=`echo $CORES|cut -d',' -f-$CORE_COUNT`
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/tcp/ix_pingpongs $MSG_SIZE >> ix.log 2>&1) &
}

server_ix_stream() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  IX_PARAMS_CORES=`echo $CORES|cut -d',' -f-$CORE_COUNT`
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/tcp/ix_server >> ix.log 2>&1) &
}

server_linux_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/pingpongs $MSG_SIZE `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

server_linux_stream() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  $DIR/server `echo $CORES|cut -d',' -f-$CORE_COUNT` &
}

run_single() {
  MSG_SIZE=$1

  $SERVER 16 $MSG_SIZE
  ssh $HOST "while ! nc -w 1 $SERVER_IP $SERVER_PORT; do sleep 1; i=\$[i+1]; if [ \$i -eq 30 ]; then exit 1; fi; done"
  echo -ne "16\t$MSG_SIZE\t999999999\t" >> $OUTDIR/data
  python $DIR/launch.py --time $TIME --clients $CLIENT_HOSTS --client-cmdline "`eval echo $CLIENT_CMDLINE`" >> $OUTDIR/data
  $ON_EXIT
}

run() {
  for i in {6..25}; do
    run_single $[2**$i]
  done
}

run_netpipe() {
  PARAMS="-r -n 100 -p 0 -l 64 -u $[2**25]"
  ./NPtcp $PARAMS > /dev/null 2>&1 &
  ssh $HOST "./NPtcp $PARAMS -h $SERVER_IP 2> /dev/null && awk '//{printf(\"0\\t%d\\t999999999\\t%f 0 0 0 0 0\\n\",\$1,\$2*1024*1024/8/2/\$1)}' np.out" > $OUTDIR/data
}

prepare
OUTDIR=`bench_start "pingpong/$SERVER_SPEC/$CLIENT_SPEC"`
trap $ON_EXIT EXIT
if [ $NETPIPE -eq 0 ]; then
  run
else
  run_netpipe
fi
bench_stop $OUTDIR
