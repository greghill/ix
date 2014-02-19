# Makefile for the Intel ixgbe driver module

SRC = ixgbe_82598.c ixgbe_82599.c ixgbe_api.c ixgbe_common.c ixgbe_dcb_82598.c \
      ixgbe_dcb_82599.c ixgbe_dcb.c ixgbe_mbx.c ixgbe_phy.c ixgbe_vf.c \
      ixgbe_x540.c ixgbe_ethdev.c ixgbe_fdir.c ixgbe_pf.c ixgbe_rxtx.c
$(eval $(call register_dir, ixgbe, $(SRC)))

