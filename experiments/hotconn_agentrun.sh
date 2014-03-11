#!/bin/bash
#################################
# Mutilate Agent Profile: HotConn
#################################
#  - Defines a total of 30 agent machines
#  - Uses all the hotbox-* machines from 11 to 40
#  - Creates a small number of hot connections for connection imbalance
###

for i in $@
do
    case $i in
        # Agent hotbox-11 will create 1 thread and use a lambda multiplier and connection depth of 20
        # Total number of hot connections is equal to the experiment's "-c" option
        hotbox-11) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 1 -A -l 32 -d 32" ;;
        
        # All other hotbox machines will create a standard high number of connections with low QPS share
        hotbox-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 8 -A -l 1" ;;
        
        # Only use hotbox machines
        *) exit 1 ;;
    esac
done
