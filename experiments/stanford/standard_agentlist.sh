#!/bin/bash
##################################
# Mutilate Agent Profile: Standard
##################################
#  - Defines a total of 39 agent machines
#  - Uses all the hotbox-* machines from 2 to 9 and 11 to 40 plus maverick-16-10g
#  - Provides no special options for any of these machines
###

seq -f "hotbox-%g" 2 9
seq -f "hotbox-%g" 11 40
echo maverick-16-10g
