#!/bin/bash
################################
# Mutilate Agent Profile: FB ETC
################################

for i in $@
do
    case $i in
        icnals*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 16 -A" ;;

        # Only use maverick machines
        *) exit 1 ;;
    esac
done
