# Makefile for network module

SRC = arp.c cfg.c dump.c icmp.c ip.c net.c udp.c
$(eval $(call register_dir, net, $(SRC)))

