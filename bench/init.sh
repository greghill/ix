#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[1]}: line ${BASH_LINENO[0]}: Failed at `date`"
}
trap on_err ERR

if [ "$UID" -ne 0 ]; then
  echo "Must be root to run this script." 2>&1
  exit 1;
fi

# conf

NOFILE=10000000
SERVER_IP=192.168.21.1
SERVER_MAC=A0:36:9F:27:3C:16

if [ -d /sys/devices/system/cpu/cpu0/cpufreq ]; then
  for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo userspace > $i
  done
  FREQ=`awk '{ if ($1 - $2 > 1000) print $1; else print $2 }' /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
  for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed; do
    echo $FREQ > $i
  done
fi

if [ $SERVER == "1" ]; then
  if [ $USE_IX == "1" ]; then
    sysctl fs.nr_open=$NOFILE > /dev/null
    if [ `ulimit -n` -lt $NOFILE ]; then
      echo 'Add the following lines into /etc/security/limits.conf and re-login.'
      echo "`whoami` soft nofile $NOFILE"
      echo "`whoami` hard nofile $NOFILE"
      exit 1
    fi
  fi
else
  if [ "$NET" = 'mtcp' ]; then
    export IP=$IX_IP
  else
    export IP=auto
  fi
  export ALL_IFS=$NIC
  export SINGLE_NIC=$NIC
  sudo -u $SUDO_USER bash -c 'cat > ix.cfg' <<EOF
host_addr $IX_IP
arp $SERVER_IP $SERVER_MAC
EOF
fi

if [ "$NET" = 'mtcp' ]; then
  bash select_net.sh $NET 16
else
  bash select_net.sh $NET
fi
