#!/bin/bash
##################################
# Mutilate Agent Profile: Standard
##################################
#  - Defines a total of 30 agent machines
#  - Uses all the hotbox-* machines from 11 to 40
#  - Provides no special options for any of these machines
###

seq -f "hotbox-%g" 11 40
