# Makefile for network module

SRC = arp.c cfg.c dump.c icmp.c ip.c net.c tcp.c tcp_in.c tcp_out.c udp.c
ifneq ($(ENABLE_PCAP),)
SRC += pcap.c
endif
$(eval $(call register_dir, net, $(SRC)))

