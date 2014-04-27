#!/bin/bash
#################################
# Mutilate Agent Profile: HotConn
#################################
#  - Defines a total of 39 agent machines
#  - Uses all the hotbox-* machines from 2 to 9 and 11 to 40 plus maverick-16-10g
#  - Creates a small number of hot connections for connection imbalance
###

seq -f "hotbox-%g" 2 9
seq -f "hotbox-%g" 11 40
echo maverick-16-10g
