#!/bin/bash
[ `id -u` = 0 ] || exec sudo "$0" "$@"

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

$(dirname $0)/set_irq_affinity.sh $1 $2

for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo userspace > $i
done

FREQ=`awk '{ if ($1 - $2 > 1000) print $1; else print $2 }' \
          /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed; do
    echo $FREQ > $i
done

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

for i in $@; do 
    ifconfig $i txqueuelen 1000
done

sleep 2
