# Makefile for network module

SRC = arp.c cfg.c dpdk.c dump.c icmp.c ipv4.c
$(eval $(call register_dir, net, $(SRC)))

