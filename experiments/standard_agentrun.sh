#!/bin/bash
##################################
# Mutilate Agent Profile: Standard
##################################
#  - Defines a total of 30 agent machines
#  - Uses all the hotbox-* machines from 11 to 40
#  - Provides no special options for any of these machines
###

for i in $@
do
    case $i in
        hotbox-*) echo "$i:${MUTILATE_EXPERIMENT_PREFIX}mutilate -T 8 -A -l 1" ;;
        *) exit 1 ;;
    esac
done
