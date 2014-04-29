#!/bin/bash
################################
# Mutilate Agent Profile: FB ETC
################################

for i in $@
do
    case $i in
        maverick-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 24 -A" ;;
        
        # Only use maverick machines
        *) exit 1 ;;
    esac
done
