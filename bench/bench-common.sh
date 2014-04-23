#!/bin/bash

bench_start() {
  name=$1
  dir=results/$(date +%Y-%m-%d-%H-%M-%S)/$name
  mkdir -p $dir
  sha1=$(git rev-parse HEAD)
  if ! git diff-index --quiet HEAD; then
    sha1="$sha1+"
  fi
  echo "SHA1: $sha1" > $dir/info
  echo "Start: $(date)" >> $dir/info
  git diff HEAD > $dir/patch
  echo $dir
}

bench_stop() {
  dir=$1
  echo "Stop: $(date)" >> $dir/info
  echo "Done" >> $dir/info
}

prepare() {
  if [ $# -le 1 ]; then
    BUILD_IX=1
    BUILD_TARGET_BENCH=
  elif [ $# -ge 2 ]; then
    BUILD_IX=$1
    BUILD_TARGET_BENCH=$2
  fi
  
  CLIENT_COUNT=0
  CLIENT_HOSTS=
  for CLIENT_DESC in $CLIENTS; do
    IFS='|'
    read -r HOST NIC <<< "$CLIENT_DESC"
    CLIENT_HOSTS="$CLIENT_HOSTS,$HOST"
    CLIENT_COUNT=$[$CLIENT_COUNT + 1]
  done
  IFS=' '
  CLIENT_HOSTS=${CLIENT_HOSTS:1}

  ## make
  $DIR/../make.sh $BUILD_IX $BUILD_TARGET_BENCH
  
  ## deploy
  # clients have shared file system
  scp $DEPLOY_FILES init.sh $HOST: > /dev/null
  
  pids=''
  sudo SERVER=1 NET="$SERVER_NET" USE_IX=$BUILD_IX bash init.sh &
  pids="$pids $!"
  for CLIENT_DESC in $CLIENTS; do
    IFS='|'
    read -r HOST NIC IX_IP <<< "$CLIENT_DESC"
    ssh $HOST "sudo SERVER=0 NET='$CLIENT_NET' USE_IX=$BUILD_IX NIC=$NIC IX_IP=$IX_IP bash init.sh" &
    pids="$pids $!"
  done
  IFS=' '

  for pid in $pids; do
    wait $pid
  done
}
