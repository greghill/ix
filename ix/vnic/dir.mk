# Makefile for virtual NIC module

SRC = virtual_nic.c
$(eval $(call register_dir, vnic, $(SRC)))

