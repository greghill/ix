#!/bin/bash

set -e

BMOS_PATH=~/epfl1/prg/me/bmos

BOND1=eth4
BOND2=eth5
BOND3=eth6
BOND4=eth7
REST1=eth8
REST2=eth9
REST3=eth10
NIC=eth11

pushd `dirname $0` > /dev/null
SCRIPTPATH=`pwd`
popd > /dev/null

common() {
  service irqbalance stop || true
  ifdown $BOND1 $BOND2 $BOND3 $BOND4
  ifdown bond0
  modprobe -r ixgbe
  rmmod dune || true
  rmmod igb_stub || true
  ifdown $BOND1 $BOND2 $BOND3 $BOND4 $NIC $REST1 $REST2 $REST3
  ifdown bond0
  echo 0 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
  echo 0 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
}

common_linux() {
  common
  service irqbalance start
  modprobe ixgbe
  ifdown $BOND2 $BOND3 $BOND4 $REST1 $REST2 $REST3 $NIC
  ifdown bond0
}

common_ix() {
  common
  (cd $BMOS_PATH/dune && make)
  (cd $BMOS_PATH/igb_stub && make)
  insmod $BMOS_PATH/dune/dune.ko
  insmod $BMOS_PATH/igb_stub/igb_stub.ko
}

if [ -z $1 ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $1 = 'none' ]; then
  common
elif [ $1 = 'bond' ]; then
  common_linux
  ifup $BOND1 $BOND2 $BOND3 $BOND4
  ifup bond0
elif [ $1 = 'single' ]; then
  common_linux
  ifconfig $NIC 192.168.21.2
elif [ $1 = 'single-onecore' ]; then
  common_linux
  ifconfig $NIC 192.168.21.2
elif [ $1 = 'ix-numa0' ]; then
  common_ix
  echo 10000 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
elif [ $1 = 'ix-numa1' ]; then
  common_ix
  echo 10000 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
else
  echo 'invalid parameter' >&2
  exit 1
fi
