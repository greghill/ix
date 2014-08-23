#!/bin/bash

MTCP_CLIENT="icnals2"
GRUB_2_6="Linux 2.6.32"

set -x

STAGE=${1:-1}

if [ $STAGE == 1 ]; then
  ./bench-pingpong.sh Netpipe-10 Netpipe
  ./bench-pingpong.sh IX-10-RPC IX

  ./bench-short.sh Linux-10-RPC Linux-Libevent
  ./bench-short.sh Linux-40-RPC Linux-Libevent
  ./bench-short.sh IX-10-RPC Linux-Libevent
  ./bench-short.sh IX-40-RPC Linux-Libevent

  ./bench-connscaling.sh Linux-10-RPC Linux-Libevent
  ./bench-connscaling.sh Linux-40-RPC Linux-Libevent
  ./bench-connscaling.sh IX-10-RPC Linux-Libevent
  ./bench-connscaling.sh IX-40-RPC Linux-Libevent

  CMD="( sudo -u $USER sh -c 'cd $PWD && nohup bash $0 2' & )# RUNALL_ENTRY\n"
  sudo sed -i -e "\$i $CMD" /etc/rc.local
  sudo grub-reboot "$GRUB_2_6"
  sudo bash ./select_net.sh linux single
  sudo reboot
elif [ $STAGE == 2 ]; then
  sudo sed -i -e '/RUNALL_ENTRY/d' /etc/rc.local
  ./bench-short.sh mTCP-10-RPC Linux-Libevent

  ssh $MTCP_CLIENT "sudo grub-reboot \"$GRUB_2_6\" && sudo reboot"
  sleep 60
  while ! ping -c 1 $MTCP_CLIENT &>/dev/null; do
    sleep 5
  done
  sleep 60

  ./bench-pingpong.sh Netpipe-mTCP Netpipe-mTCP

  ssh $MTCP_CLIENT 'sudo reboot'
  sudo reboot
fi
