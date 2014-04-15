#!/bin/bash
[ `id -u` = 0 ] || exec sudo "$0" "$@"

WAKEUP_GRANULARITY=4000000
NOATR="AtrSampleRate=1,1"

while getopts 'ihrRw:' OPTION; do
      case $OPTION in
          i) NOIC=1 ;;
          r) NOATR="AtrSampleRate=0,0" ;;
          R) NOATR="AtrSampleRate=0,0 RSS=12,12" ;;
#          r) NOATR="AtrSampleRate=0" ;;
          w) WAKEUP_GRANULARITY=$[OPTARG * 1000] ;;
          ?) cat $0; exit ;;
      esac
done
shift $[OPTIND - 1]

if [ "$NOIC" = 1 ]; then
    ITR="InterruptThrottleRate=0,0"
    IC="rx-usecs 0 tx-frames-irq 1"
fi

DEVS="p3p1"
#if [ -x /sys/class/net/p1p2 ]; then DEVS="p1p1 p1p2"; fi

/etc/init.d/puppet stop
stop cron
stop irqbalance
/etc/init.d/ganglia-monitor stop
/etc/init.d/openafs-client stop
/etc/init.d/uml-utilities stop
/etc/init.d/postfix stop
killall console-kit-daemon
killall polkitd
umount /gluster
stop atd
/etc/init.d/rwhod stop
/etc/init.d/torque-mom stop #tends to fail
/etc/init.d/ntp stop
stop cgred

#ethtool -C eth0 rx-usecs 0

for i in $DEVS; do ifdown $i; done

#ifdown p1p2
#ifdown eth3

sleep 1

rmmod ixgbe

modprobe ixgbe $ITR $NOATR "$@"
echo modprobe ixgbe $ITR $NOATR "$@"

sleep 3

#ethtool -C eth2 rx-usecs 0 tx-frames-irq 1

for i in $DEVS; do ethtool -C $i $IC; done

#ethtool -K eth2 rxhash on
#ethtool -K eth3 rxhash on
#ethtool -K p1p2 rxhash on

sleep 3

for i in $DEVS; do $(dirname $0)/set_irq_affinity.sh $i; done

for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo userspace > $i
done

FREQ=`awk '{ if ($1 - $2 > 1000) print $1; else print $2 }' \
          /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed; do
    echo $FREQ > $i
done

killall pmqos
nohup $(dirname $0)/pmqos -c 1 < /dev/null &> /dev/null &
sleep 1

sysctl net.core.netdev_max_backlog=1000

sysctl net.core.wmem_max=$[128*1024]
sysctl net.core.rmem_max=$[128*1024]
sysctl net.core.wmem_default=126976
sysctl net.core.rmem_default=126976

sysctl net.ipv4.tcp_rmem="4096 87380 4194304"
sysctl net.ipv4.tcp_wmem="4096 87380 4194304"

sysctl net.core.somaxconn=32768
sysctl net.ipv4.tcp_max_syn_backlog=32768
sysctl net.ipv4.tcp_syncookies=0

for i in $DEVS; do 
    ifconfig $i txqueuelen 1000
done

sleep 2

