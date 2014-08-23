# Makefile for network module

SRC = arp.c dump.c icmp.c ip.c net.c tcp.c tcp_in.c tcp_out.c \
      tcp_api.c udp.c
ifneq ($(ENABLE_PCAP),)
SRC += pcap.c
endif
$(eval $(call register_dir, net, $(SRC)))

