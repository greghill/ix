# Makefile for network module

SRC = arp.c cfg.c dpdk.c dpdk_nic.c dump.c icmp.c ipv4.c main.c periodic_ping_nic.c
$(eval $(call register_dir, net, $(SRC)))

