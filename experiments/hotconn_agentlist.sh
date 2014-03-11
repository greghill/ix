#!/bin/bash
#################################
# Mutilate Agent Profile: HotConn
#################################
#  - Defines a total of 30 agent machines
#  - Uses all the hotbox-* machines from 11 to 40
#  - Creates a small number of hot connections for connection imbalance
###

seq -f "hotbox-%g" 11 40
