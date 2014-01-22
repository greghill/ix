# A Makefile for IX.
#
# NOTE: The environment variable DPDK_SDK must be set to the DPDK install
# path for this Makefile to work properly.

ifeq ($(DPDK_SDK),)
$(error "Please set the DPDK_SDK environment variable to your DPDK path")
endif

DPDK_INC_PATH	= $(DPDK_SDK)/build/include
DPDK_LIB_PATH	= $(DPDK_SDK)/build/lib
DPDK_LIBS	= ethdev rte_cmdline rte_hash rte_kni rte_lpm \
		  rte_malloc rte_mempool rte_meter rte_mbuf \
		  rte_pmd_e1000 rte_pmd_ixgbe rte_pmd_virtio \
		  rte_power rte_ring rte_sched rte_timer rte_eal rte_pmd_ring

CC	= gcc
CFLAGS	= -g -Wall -O3 -I$(DPDK_INC_PATH) -I./inc
LD	= gcc
LDFLAGS	=
LDLIBS	= -L$(DPDK_LIB_PATH) -lrt -ldl -lpthread -lm $(addprefix -l, $(DPDK_LIBS))

SRCS =
DIRS = core net ixgbe

define register_dir
SRCS += $(patsubst %, $(1)/%, $(2))
endef

include $(patsubst %, %/dir.mk, $(DIRS))

OBJS=$(subst .c,.o,$(SRCS))

all: ix

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(foreach SRC,$(SRCS),$(CC) -MM -MT $(SRC:.c=.o) $(SRC) >> .depend;)

include .depend

ix: $(OBJS)
	$(LD) $(LDFLAGS) -o ix $(OBJS) $(LDLIBS)

clean:
	rm $(OBJS) ix .depend

dist-clean: clean
	rm *~

