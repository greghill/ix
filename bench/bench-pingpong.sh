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
  SERVER_IP=192.168.21.1
  PS_IXGBE_PATH=~/mtcp/io_engine/driver
elif [ $CLUSTER_ID = 'Stanford' ]; then
  CLIENTS="$CLIENTS maverick-10|p7p1|10.79.6.21"
  SERVER_IP=10.79.6.26
elif [ $CLUSTER_ID = 'Stanford-IX' ]; then
  CLIENTS="$CLIENTS maverick-12|p3p1|10.79.6.23"
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
  echo "              | Netpipe-10 | Netpipe-40 | mTCP-10-RPC | mTCP-10-Stream"
  echo "              | Netpipe-10-Optimized | Netpipe-40-Optimized"
  echo "              | Netpipe-mTCP"
  echo "  CLIENT_SPEC = Linux-Libevent|Linux-Simple|Netpipe"
  echo "              | Netpipe-Optimized | Netpipe-mTCP | IX"
  echo "  CLUSTER_ID  = EPFL|Stanford (default: EPFL)"
  exit
fi

SERVER_SPEC=$1
CLIENT_SPEC=$2

NETPIPE=0

if [ -z $SERVER_SPEC ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $SERVER_SPEC = 'IX-10-RPC' ]; then
  SERVER=server_ix_rpc
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  BUILD_IX=1
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
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'Linux-10-RPC' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'mTCP-10-RPC' ]; then
  SERVER_NET="mtcp"
  SERVER=server_mtcp_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_mtcp_rpc
  CORES="0"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Linux-10-Stream' ]; then
  SERVER_NET="linux single"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'mTCP-10-Stream' ]; then
  SERVER_NET="mtcp"
  SERVER=server_mtcp_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_mtcp_stream
  CORES="0"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Linux-40-RPC' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_rpc
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_rpc
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Linux-40-Stream' ]; then
  SERVER_NET="linux bond"
  SERVER=server_linux_stream
  SERVER_PORT=9876
  ON_EXIT=on_exit_linux_stream
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'IX-10-Stream' ]; then
  SERVER_NET="ix node1"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="1,17,3,19,5,21,7,23,9,25,11,27,13,29,15,31"
  BUILD_IX=1
  IX_PARAMS="-d 0000:42:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'IX-40-Stream' ]; then
  SERVER_NET="ix node0"
  SERVER=server_ix_stream
  SERVER_PORT=8000
  ON_EXIT=on_exit_ix
  CORES="0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
  BUILD_IX=1
  IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c \$IX_PARAMS_CORES"
elif [ $SERVER_SPEC = 'Netpipe-10' ]; then
  SERVER_NET="linux single"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_SERVER="NPtcp"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Netpipe-40' ]; then
  SERVER_NET="linux bond"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_SERVER="NPtcp"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Netpipe-10-Optimized' ]; then
  SERVER_NET="linux single opt"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_SERVER="NPtcp"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Netpipe-40-Optimized' ]; then
  SERVER_NET="linux bond opt"
  ON_EXIT=on_exit_netpipe
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_SERVER="NPtcp"
  BUILD_IX=0
elif [ $SERVER_SPEC = 'Netpipe-mTCP' ]; then
  SERVER_NET="mtcp"
  ON_EXIT=on_exit_netpipe_mtcp
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_SERVER="NPmtcp"
  BUILD_IX=0
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
  KILL_CLIENT="killall -KILL client 2> /dev/null || true"
elif [ $CLIENT_SPEC = 'Linux-Simple' ]; then
  THREADS=1
  CONNECTIONS_PER_THREAD=1
  CLIENT_CMDLINE="./simple_client $SERVER_IP $SERVER_PORT $THREADS $CONNECTIONS_PER_THREAD \$MSG_SIZE 999999999 0 0"
  DEPLOY_FILES="select_net.sh simple_client"
  CLIENT_NET="linux single"
  KILL_CLIENT="killall -KILL simple_client 2> /dev/null || true"
elif [ $CLIENT_SPEC = 'Netpipe' ]; then
  DEPLOY_FILES="select_net.sh NPtcp"
  CLIENT_NET="linux single"
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_CLIENT="NPtcp"
  KILL_CLIENT=""
elif [ $CLIENT_SPEC = 'Netpipe-Optimized' ]; then
  DEPLOY_FILES="select_net.sh NPtcp"
  CLIENT_NET="linux single opt"
  KILL_CLIENT=""
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_CLIENT="NPtcp"
elif [ $CLIENT_SPEC = 'Netpipe-mTCP' ]; then
  DEPLOY_FILES="select_net.sh NPmtcp NPmtcp.conf $PS_IXGBE_PATH/ps_ixgbe.ko"
  CLIENT_NET="mtcp"
  KILL_CLIENT=""
  NETPIPE=$[$NETPIPE+1]
  NETPIPE_EXEC_CLIENT="NPmtcp"
elif [ $CLIENT_SPEC = 'IX' ]; then
  CLIENT_CMDLINE=" sudo ./ix -q -d 0000:01:00.0 -c 0 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ./client_test $SERVER_IP $SERVER_PORT \$MSG_SIZE"
  DEPLOY_FILES="select_net.sh ../ix/ix ../apps/event/client_test ../dune/dune.ko ../igb_stub/igb_stub.ko"
  CLIENT_NET="ix node0"
  KILL_CLIENT="sudo killall -KILL ix 2> /dev/null || true"
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

on_exit_common() {
  if [ -z "$KILL_CLIENT" ]; then return; fi
  IFS=',' read -ra HOSTS <<< $CLIENT_HOSTS
  for HOST in "${HOSTS[@]}"; do
    ssh $HOST "$KILL_CLIENT"
  done
}

on_exit_ix() {
  on_exit_common
  PID=`pidof ix||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  sudo kill -KILL $PID 2>/dev/null || true
}

on_exit_linux_rpc() {
  on_exit_common
  PID=`pidof pingpongs||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

on_exit_mtcp_rpc() {
  on_exit_common
  PID=`pidof pingpongs_mtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
  rm -f log_*
}

on_exit_linux_stream() {
  on_exit_common
  PID=`pidof server||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

on_exit_mtcp_stream() {
  on_exit_common
  PID=`pidof server_mtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
  rm -f log_*
}

on_exit_netpipe() {
  on_exit_common
  PID=`pidof NPtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

on_exit_netpipe_mtcp() {
  on_exit_common
  PID=`pidof NPmtcp||echo 0`
  if [ $PID -eq 0 ]; then return; fi
  kill $PID
  wait $PID 2>/dev/null || true
}

server_ix_rpc() {
  CORE_COUNT=$1
  MSG_SIZE=$2

  IX_PARAMS_CORES=`echo $CORES|cut -d',' -f-$CORE_COUNT`
  (trap ERR; cd $DIR/../ix; sudo stdbuf -o0 -e0 ./ix `eval echo $IX_PARAMS` /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/event/pingpong $MSG_SIZE 2 >> ix.log 2>&1) &
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
  MSG_SIZE=$1
  if [ $CLUSTER_ID = 'Stanford-IX' ]; then
    $SERVER 6 $MSG_SIZE
  elif [ $CLUSTER_ID = 'EPFL' ]; then
    $SERVER 16 $MSG_SIZE
  fi

  if [ $CLIENT_SPEC != 'IX' ]; then
    ssh $HOST "while ! nc -w 1 $SERVER_IP $SERVER_PORT; do sleep 1; i=\$[i+1]; if [ \$i -eq 30 ]; then exit 1; fi; done"
  fi
  echo -ne "16\t$MSG_SIZE\t999999999\t" >> $OUTDIR/data
  python $DIR/launch.py --time $TIME --clients $CLIENT_HOSTS --client-cmdline "`eval echo $CLIENT_CMDLINE`" >> $OUTDIR/data
  sleep 2
  $ON_EXIT
}

run() {
  run_single 64
  for i in {1..9}; do
    run_single $[$i * 20000]
  done
  for i in {2..4}; do
    run_single $[$i * 100000]
  done
  run_single $[512 * 1024]
}

run_netpipe() {
  PARAMS="-r -n 100 -p 0 -l 64 -u $[2**20]"
  ./$NETPIPE_EXEC_SERVER $PARAMS > /dev/null 2>&1 &
  sleep 2
  ssh $HOST "./$NETPIPE_EXEC_CLIENT $PARAMS -h $SERVER_IP > /dev/null 2>&1 && awk '//{printf(\"0\\t%d\\t999999999\\t%f 0 0 0 0 0\\n\",\$1,\$2*1024*1024/8/2/\$1)}' np.out" > $OUTDIR/data
}

prepare $BUILD_IX
OUTDIR=`bench_start "pingpong/$SERVER_SPEC/$CLIENT_SPEC"`
trap $ON_EXIT EXIT
if [ $NETPIPE -eq 0 ]; then
  run
else
  run_netpipe
fi
bench_stop $OUTDIR
