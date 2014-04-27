#!/bin/bash

set -eEu -o pipefail

on_err() {
  echo "${BASH_SOURCE[1]}: line ${BASH_LINENO[0]}: Failed at `date`"
}
trap on_err ERR


# Validate arguments
if [ $# -lt 1 -o $# -gt 2 ]
then
    echo "Usage: $0 SERVER_SPEC [CLUSTER_ID]" >&2
    echo "  SERVER_SPEC = Linux-10 | Linux-40 | IX-10 | IX-40"
    echo "  CLUSTER_ID = EPFL | Stanford (default: EPFL)"
    exit 1
fi

SERVER_SPEC=$1

if [ $# -eq 2 ]; then
  CLUSTER_ID=$2
else
  CLUSTER_ID='EPFL'
fi


# Configure
if [ $CLUSTER_ID = 'EPFL' ]; then
  export AGENT_SUBDIR="epfl/"
  echo "EPFL: please specify a server!" >&2 && false
  SERVER_HOST=
elif [ $CLUSTER_ID = 'Stanford' ]; then
  export AGENT_SUBDIR="stanford/"
  SERVER_HOST="maverick-17-10g"
else
  echo 'Invalid paramters' >&2
  exit 1
fi

if [ $SERVER_SPEC = 'Linux-10' ]; then
  OUTDIR='Linux-10'
elif [ $SERVER_SPEC = 'Linux-40' ]; then
  OUTDIR='Linux-40'
elif [ $SERVER_SPEC = 'IX-10' ]; then
  OUTDIR='IX-10'
elif [ $SERVER_SPEC = 'IX-40' ]; then
  OUTDIR='IX-40'
else
  echo 'Invalid parameters' >&2
  exit 1
fi

# Run experiments
OUTDIR_BASE="results/$(date +%Y-%m-%d-%H-%M-%S)"
OUTDIR=$OUTDIR_BASE/$OUTDIR
mkdir -p $OUTDIR
./run_experiment.sh $SERVER_HOST fb_etc > $OUTDIR/fb_etc.csv
./run_experiment.sh $SERVER_HOST fb_usr > $OUTDIR/fb_usr.csv
