#!/bin/bash

NOFILE=10000000

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

  ### make
  $DIR/../make.sh

  ### deploy
  # clients have shared file system
  scp $DEPLOY_FILES $HOST: > /dev/null

  ### prepare network and run init script
  INIT_SCRIPT="
if [ -d /sys/devices/system/cpu/cpu0/cpufreq ]; then
  for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo userspace > \$i
  done
  FREQ=`awk '{ if (\$1 - \$2 > 1000) print \$1; else print \$2 }' /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
  for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed; do
    echo \$FREQ > \$i
  done
fi
"

  sudo bash select_net.sh $SERVER_NET &
  sudo bash <<< "$INIT_SCRIPT"
  sudo sysctl fs.nr_open=$NOFILE > /dev/null
  if [ `ulimit -n` -lt $NOFILE ]; then
    echo 'Add the following lines into /etc/security/limits.conf and re-login.'
    echo "`whoami` soft nofile $NOFILE"
    echo "`whoami` hard nofile $NOFILE"
    exit 1
  fi
  for CLIENT_DESC in $CLIENTS; do
    IFS='|'
    read -r HOST NIC <<< "$CLIENT_DESC"
    ssh $HOST "sudo ALL_IFS=$NIC SINGLE_NIC=$NIC IP=auto bash select_net.sh $CLIENT_NET" &
    ssh $HOST '/bin/bash' <<< "$INIT_SCRIPT" &
  done
  IFS=' '

  wait
}
