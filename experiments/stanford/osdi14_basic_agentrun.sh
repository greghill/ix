#!/bin/bash
################################
# Mutilate Agent Profile: OSDI14
################################

for i in $@
do
    case $i in
        maverick-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 24 -A" ;;
        
        hotbox-*)   echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 8 -A" ;;
        
        *) exit 1 ;;
    esac
done
