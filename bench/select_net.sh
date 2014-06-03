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

# set defaults values from configuration file, if it exists
if [ -f ~/.select_net.conf ]; then
  . ~/.select_net.conf
fi

# set default values if not found in the environment
foo=${DUNE_PATH:=$DEF_DUNE_PATH}
foo=${IGB_STUB_PATH:=$DEF_IGB_STUB_PATH}
foo=${ALL_IFS:=$DEF_ALL_IFS}
foo=${BOND_SLAVES:=$DEF_BOND_SLAVES}
foo=${SINGLE_NIC:=$DEF_SINGLE_NIC}
foo=${IP:=$DEF_IP}
foo=${PAGE_COUNT_2MB:=$DEF_PAGE_COUNT_2MB}
foo=${NIC_MODULE:=$DEF_NIC_MODULE}
foo=${DEVICE:=$DEF_DEVICE}
foo=${PS_IXGBE_PATH:=$DEF_PS_IXGBE_PATH}

tear_down() {
  service irqbalance stop >/dev/null 2>&1 || true
  ifdown $ALL_IFS 2>/dev/null
  modprobe -r $NIC_MODULE
  rmmod dune 2>/dev/null || true
  rmmod igb_stub 2>/dev/null || true
  rmmod ps_ixgbe 2>/dev/null || true
  for i in /sys/devices/system/node/node*/hugepages/hugepages-2048kB/nr_hugepages; do
    echo 0 > $i
  done
  rm -f /dev/packet_shader
}

setup_linux() {
  tear_down
  service irqbalance start >/dev/null
  modprobe $NIC_MODULE
  ifdown $ALL_IFS 2>/dev/null

  sysctl net.core.netdev_max_backlog=1000 > /dev/null
  sysctl net.core.rmem_max=212992 > /dev/null
  sysctl net.core.wmem_max=212992 > /dev/null
  sysctl net.ipv4.tcp_adv_win_scale=1 > /dev/null
  sysctl net.ipv4.tcp_app_win=31 > /dev/null
  sysctl net.ipv4.tcp_congestion_control=cubic > /dev/null
  sysctl net.ipv4.tcp_rmem='4096 87380 6291456' > /dev/null
  sysctl net.ipv4.tcp_tso_win_divisor=3 > /dev/null
  sysctl net.ipv4.tcp_wmem='4096 16384 4194304' > /dev/null
}

optimize_linux() {
  sysctl net.core.netdev_max_backlog=30000 > /dev/null
  sysctl net.core.rmem_max=16777216 > /dev/null
  sysctl net.core.wmem_max=16777216 > /dev/null
  sysctl net.ipv4.tcp_adv_win_scale=31 > /dev/null
  sysctl net.ipv4.tcp_app_win=1 > /dev/null
  sysctl net.ipv4.tcp_congestion_control=htcp > /dev/null
  sysctl net.ipv4.tcp_rmem='262144 262144 6291456' > /dev/null
  sysctl net.ipv4.tcp_tso_win_divisor=1 > /dev/null
  sysctl net.ipv4.tcp_wmem='262144 262144 6291456' > /dev/null
}

setup_ix() {
  tear_down
  insmod $DUNE_PATH/dune.ko
  insmod $IGB_STUB_PATH/igb_stub.ko
}

setup_mtcp() {
  QUEUES=$1
  tear_down
  insmod $PS_IXGBE_PATH/ps_ixgbe.ko RXQ=$QUEUES TXQ=$QUEUES device=$DEVICE
  ethtool -A xge0 autoneg off rx off tx off
  ifconfig xge0 $IP mtu 1500 netmask 255.255.255.0
  mknod /dev/packet_shader c 1010 0
  chmod 666 /dev/packet_shader
}

invalid_params() {
  echo 'invalid parameters' >&2
  exit 1
}

print_help() {
  echo "Usage:"
  echo "  $0 none"
  echo "  $0 linux [single|bond] [opt]"
  echo "  $0 ix    [node0|node1|...]"
  echo "  $0 mtcp  QUEUES"
}

if [ $# -lt 1 ]; then
  print_help
  exit
fi

if [ -z $1 ]; then
  echo 'missing parameter' >&2
  exit 1
elif [ $1 = 'none' ]; then
  tear_down
elif [[ $1 == 'linux' && $# -gt 1 && "$2" == 'single' && ( $# -lt 3 || "$3" == 'opt' ) ]]; then
  setup_linux
  if [ $IP = 'auto' ]; then
    ifup $SINGLE_NIC >/dev/null
  else
    ifconfig $SINGLE_NIC $IP
  fi
  if [[ $# -ge 3 && "$3" == 'opt' ]]; then
    optimize_linux
  fi
elif [[ $1 == 'linux' && $# -gt 1 && "$2" == 'bond' && ( $# -lt 3 || "$3" == 'opt' ) ]]; then
  setup_linux
  ifup $BOND_SLAVES >/dev/null
  if [[ $# -ge 3 && "$3" == 'opt' ]]; then
    optimize_linux
  fi
elif [ $1 = 'ix' ]; then
  SYSFILE=/sys/devices/system/node/$2/hugepages/hugepages-2048kB/nr_hugepages
  if [ -f $SYSFILE ]; then
    setup_ix
    echo $PAGE_COUNT_2MB > $SYSFILE
  else
    invalid_params
  fi
elif [[ $1 == 'mtcp' && $# -gt 1 ]]; then
  setup_mtcp $2
elif [ $1 = '--help' ]; then
  print_help
else
  invalid_params
fi
