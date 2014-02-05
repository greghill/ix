SRC = main.c umm.c trap.c exec.c
$(eval $(call register_dir, sandbox, $(SRC)))
