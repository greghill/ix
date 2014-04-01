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
        # Supply standard options to the maverick-16 agent machine
        maverick-16-10g) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 24 -A -l 1" ;;
        
        # Supply standard options to all agent hotbox machines
        hotbox-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}/mutilate -T 8 -A -l 1" ;;
        
        # Only use hotbox machines
        *) exit 1 ;;
    esac
done
