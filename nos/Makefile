# A Makefile for NOS (Network OS).
#
# NOTE: The environment variable DPDK_SDK must be set to the DPDK install
# path for this Makefile to work properly.

ifeq ($(DPDK_SDK),)
$(error "Please set the DPDK_SDK environment variable to your DPDK path")
endif

DPDK_INC_PATH	= $(DPDK_SDK)/build/include
DPDK_LIB_PATH	= $(DPDK_SDK)/build/lib
DPDK_LIBS	= ethdev rte_cmdline rte_eal rte_hash rte_kni rte_lpm \
		  rte_malloc rte_mbuf rte_mempool rte_meter \
		  rte_pmd_e1000 rte_pmd_ixgbe rte_power rte_ring \
		  rte_sched rte_timer

CC	= gcc
CFLAGS	= -g -O3 -I$(DPDK_INC_PATH)
LD	= ld
LDFLAGS	=
LDLIBS	= -L$(DPDK_LIB_PATH) -lm $(addprefix -l, $(DPDK_LIBS))

SRCS=dpdk.c
OBJS=$(subst .c,.o,$(SRCS))

all: nos

nos: $(OBJS)
	$(LD) $(LDFLAGS) -o nos $(OBJS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	rm -rf ./.depend
	$(CC) $(CFLAGS) -MM $^>>./.depend;

clean:
	rm $(OBJS) nos

dist-clean: clean
	rm *~ .dependnos

include .depend

