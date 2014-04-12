#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[0]}: line ${BASH_LINENO[0]}: Failed"
}
trap on_err ERR

# set default values if not found in the environment
foo=${BMOS_PATH:=~/epfl1/prg/me/bmos}
foo=${ALL_IFS:="eth4 eth5 eth6 eth7 eth8 eth9 eth10 eth11 bond0"}
foo=${BOND:=bond0}
foo=${BOND_SLAVES:="eth4 eth5 eth6 eth7"}
foo=${SINGLE_NIC:=eth11}
foo=${IP:=192.168.21.1}
foo=${PAGE_COUNT_2MB:=10000}

tear_down() {
  service irqbalance stop >/dev/null 2>&1 || true
  ifdown $ALL_IFS 2>/dev/null
  modprobe -r ixgbe
  rmmod dune 2>/dev/null || true
  rmmod igb_stub 2>/dev/null || true
  for i in /sys/devices/system/node/node*/hugepages/hugepages-2048kB/nr_hugepages; do
    echo 0 > $i
  done
}

setup_linux() {
  tear_down
  service irqbalance start >/dev/null
  modprobe ixgbe
  ifdown $ALL_IFS 2>/dev/null
}

setup_ix() {
  tear_down
  (cd $BMOS_PATH/dune && make -j64 >/dev/null 2>&1)
  (cd $BMOS_PATH/igb_stub && make -j64 >/dev/null)
  insmod $BMOS_PATH/dune/dune.ko
  insmod $BMOS_PATH/igb_stub/igb_stub.ko
}

invalid_params() {
  echo 'invalid parameters' >&2
  exit 1
}

print_help() {
  echo "Usage:"
  echo "  $0 none"
  echo "  $0 linux [single|bond]"
  echo "  $0 ix    [node0|node1|...]"
}

if [ $# -lt 2 ]; then
  print_help
  exit
fi

if [ -z $1 ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $1 = 'none' ]; then
  tear_down
elif [ $1 = 'linux' -a "$2" = 'single' ]; then
  setup_linux
  if [ $IP = 'auto' ]; then
    ifup $SINGLE_NIC >/dev/null
  else
    ifconfig $SINGLE_NIC $IP
  fi
elif [ $1 = 'linux' -a "$2" = 'bond' ]; then
  setup_linux
  ifup $BOND_SLAVES >/dev/null
elif [ $1 = 'ix' ]; then
  SYSFILE=/sys/devices/system/node/$2/hugepages/hugepages-2048kB/nr_hugepages
  if [ -f $SYSFILE ]; then
    setup_ix
    echo $PAGE_COUNT_2MB > $SYSFILE
  else
    invalid_params
  fi
elif [ $1 = '--help' ]; then
  print_help
else
  invalid_params
fi
