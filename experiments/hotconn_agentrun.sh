#!/bin/bash
#################################
# Mutilate Agent Profile: HotConn
#################################
#  - Defines a total of 39 agent machines
#  - Uses all the hotbox-* machines from 2 to 9 and 11 to 40 plus maverick-16-10g
#  - Creates a small number of hot connections for connection imbalance
###

for i in $@
do
    case $i in
        # Agent maverick-16 will create 1 thread and use a lambda multiplier and connection depth of 32
        # Total number of hot connections (from maverick-16) is equal to the experiment's "-c" option
        maverick-16-10g) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 1 -A -l 16 -d 16" ;;
        
        # All hotbox machines will create a standard high number of connections with low QPS share
        hotbox-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 8 -A -l 1" ;;
        
        # Only use hotbox machines plus maverick-16-10g
        *) exit 1 ;;
    esac
done
