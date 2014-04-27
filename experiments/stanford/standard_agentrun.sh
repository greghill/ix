#!/bin/bash
##################################
# Mutilate Agent Profile: Standard
##################################
#  - Defines a total of 39 agent machines
#  - Uses all the hotbox-* machines from 2 to 9 and 11 to 40 plus maverick-16-10g
#  - Provides no special options for any of these machines
###

for i in $@
do
    case $i in
        # Agent maverick-16 will create 1 thread but otherwise standard options, to ensure consistency with other profiles
        # Total number of connections from maverick-16 is equal to the experiment's "-c" option
        maverick-16-10g) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 1 -A -l 1" ;;
        
        # Supply standard options to all agent hotbox machines
        hotbox-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 8 -A -l 1" ;;
        
        # Only use hotbox machines
        *) exit 1 ;;
    esac
done
