# Makefile for network module

SRC = arp.c cfg.c dump.c icmp.c ip.c net.c
$(eval $(call register_dir, net, $(SRC)))

