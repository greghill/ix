#!/bin/bash

on_exit() {
  if [ ! -z ${SERVER_HOST+x} ]; then
    ssh $SERVER_HOST killall $MEMCACHED_EXEC > /dev/null 2>&1
  fi
  
  rm -f ./mutilate
}

on_err() {
  echo "${BASH_SOURCE[1]}: line ${BASH_LINENO[0]}: Failed at `date`"
  on_exit
}

prep_linux() {
  scp memcached_prep_linux_lite.sh $SERVER_HOST:memcached_prep_linux_lite.sh
  scp set_irq_affinity.sh $SERVER_HOST:set_irq_affinity.sh
  ssh $SERVER_HOST sudo ./memcached_prep_linux_lite.sh $SERVER_IF
}

prep_linux-40() {
  scp memcached_prep_linux_lite.sh $SERVER_HOST:memcached_prep_linux_lite.sh
  scp set_irq_affinity.sh $SERVER_HOST:set_irq_affinity.sh
  ssh $SERVER_HOST sudo ./memcached_prep_linux_lite.sh $SERVER_IF
}

prep_ix() {
  echo "IX: need to implement preparation script!" && false
}

prep_ix-40() {
  echo "IX: need to implement preparation script!" && false
}

run_experiment() {
  ./prep_experiment.sh $SERVER_HOST $1
  ./run_experiment.sh $SERVER_HOST $1 > $OUTDIR/$1.csv
}

setup_and_run() {
  set -eEu -o pipefail
  trap on_err ERR
  
  # Validate arguments
  if [ $# -lt 1 -o $# -gt 2 ]
  then
      echo "Usage: $0 SERVER_SPEC [CLUSTER_ID]"
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
    echo "EPFL: please specify a server and make agent profiles!" && false
    export AGENT_SUBDIR="epfl/"
    SERVER_HOST=
    SERVER_IF=
    MEMCACHED_THREADS=32
  elif [ $CLUSTER_ID = 'Stanford' ]; then
    export AGENT_SUBDIR="stanford/"
    SERVER_HOST="maverick-17-10g"
    SERVER_IF="p3p1"
    MEMCACHED_THREADS=24
  else
    echo 'Invalid paramters'
    exit 1
  fi
  
  export MUTILATE_EXPERIMENT_PREFIX='.'
  MUTILATE_BUILD_PATH='../apps/mutilate'
  
  if [ $SERVER_SPEC = 'Linux-10' ]; then
    PREP=prep_linux
    OUTDIR='Linux-10'
    MEMCACHED_EXEC='memcached'
    MEMCACHED_BUILD_PATH='../apps/memcached-1.4.18'
    MEMCACHED_BUILD_TARGET=''
  elif [ $SERVER_SPEC = 'Linux-40' ]; then
    PREP=prep_linux-40
    OUTDIR='Linux-40'
    MEMCACHED_EXEC='memcached'
    MEMCACHED_BUILD_PATH='../apps/memcached-1.4.18'
    MEMCACHED_BUILD_TARGET=''
  elif [ $SERVER_SPEC = 'IX-10' ]; then
    echo "IX: please specify a memcached executable!" && false
    PREP=prep_ix
    OUTDIR='IX-10'
    MEMCACHED_EXEC=
    MEMCACHED_BUILD_PATH=
    MEMCACHED_BUILD_TARGET=''
  elif [ $SERVER_SPEC = 'IX-40' ]; then
    echo "IX: please specify a memcached executable!" && false
    PREP=prep_ix-40
    OUTDIR='IX-40'
    MEMCACHED_EXEC=
    MEMCACHED_BUILD_PATH=
    MEMCACHED_BUILD_TARGET=''
  else
    echo 'Invalid parameters'
    exit 1
  fi
  
  # Build, deploy, and run memcached
  if [ ! -e $MEMCACHED_BUILD_PATH/Makefile ]; then
    DIR=`pwd`
    cd $MEMCACHED_BUILD_PATH
    ./configure
    cd $DIR
  fi
  make -C $MEMCACHED_BUILD_PATH
  scp $MEMCACHED_BUILD_PATH/$MEMCACHED_EXEC $SERVER_HOST:$MEMCACHED_EXEC
  
  $PREP
  ssh $SERVER_HOST sudo nice -n -20 ./$MEMCACHED_EXEC -t $MEMCACHED_THREADS -m 8192 -c 65535 -T -u `whoami` &
  
  # Build and deploy mutilate
  DIR=`pwd`
  cd $MUTILATE_BUILD_PATH
  scons
  cd $DIR
  cp $MUTILATE_BUILD_PATH/mutilate .
  
  for i in `$AGENT_SUBDIR/standard_agentlist.sh`; do
      scp $MUTILATE_BUILD_PATH/mutilate $i:mutilate
  done
  
  # Set up the output directory
  OUTDIR_BASE="results/$(date +%Y-%m-%d-%H-%M-%S)"
  OUTDIR=$OUTDIR_BASE/$OUTDIR
  mkdir -p $OUTDIR
  
  # Run the experiments then clean up before exiting
  experiments
  on_exit
}
