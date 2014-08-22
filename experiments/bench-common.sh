#!/bin/bash

on_exit() {
  if [ ! -z ${SERVER_HOST+x} ]; then
    if [ $IX -eq 1 ]; then
      if [ $MEMCACHED_SHOULD_DEPLOY -eq 1 ]; then
        ssh $SERVER_HOST sudo killall -KILL ix > /dev/null 2>&1 || true
      fi
    else
      ssh $SERVER_HOST sudo killall $MEMCACHED_EXEC > /dev/null 2>&1 || true
    fi
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
  ssh $SERVER_HOST sudo ./memcached_prep_linux_lite.sh $SERVER_IF $MEMCACHED_CORES
}

prep_linux-40() {
  scp memcached_prep_linux_lite.sh $SERVER_HOST:memcached_prep_linux_lite.sh
  scp set_irq_affinity.sh $SERVER_HOST:set_irq_affinity.sh
  ssh $SERVER_HOST sudo ./memcached_prep_linux_lite.sh $SERVER_IF
}

prep_ix() {
  ssh $SERVER_HOST sudo ~/bmos/bench/select_net.sh ix node1
}

prep_ix-40() {
  ssh $SERVER_HOST sudo ~/bmos/bench/select_net.sh ix node0
}

run_experiment() {
  ./prep_experiment.sh $SERVER_HOST_EXPERIMENT $1
  ./run_experiment.sh $SERVER_HOST_EXPERIMENT $1 > $OUTDIR/$1.csv
}

setup_and_run() {
  set -eEu -o pipefail
  trap on_err ERR
  trap on_exit EXIT SIGHUP SIGINT SIGQUIT SIGTERM
  
  # Validate arguments
  if [ $# -lt 1 -o $# -gt 2 ]
  then
      echo "Usage: $0 SERVER_SPEC [CLUSTER_ID]"
      echo "  SERVER_SPEC = Linux-10 | Linux-40 | IX-10 | IX-40"
      echo "  CLUSTER_ID = EPFL | Stanford | Stanford-IX (default: EPFL)"
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
    SERVER_HOST=sciicebpc1
    SERVER_HOST_EXPERIMENT=192.168.21.1
    SERVER_IF=eth11
    MEMCACHED_THREADS=6
    MEMCACHED_CORES=1,3,5,7,9,11
  elif [ $CLUSTER_ID = 'Stanford' ]; then
    export AGENT_SUBDIR="stanford/"
    SERVER_HOST="maverick-17"
    SERVER_HOST_EXPERIMENT="maverick-17-10g"
    SERVER_IF="p3p1"
    MEMCACHED_THREADS=12
    MEMCACHED_CORES=0,1,2,3,4,5,12,13,14,15,16,17
  elif [ $CLUSTER_ID = 'Stanford-IX' ]; then
    export AGENT_SUBDIR="stanford/"
    SERVER_HOST="maverick-13"
    SERVER_HOST_EXPERIMENT="10.79.6.125"
    SERVER_IF=
    MEMCACHED_THREADS=
    MEMCACHED_CORES=
  else
    echo 'Invalid paramters'
    exit 1
  fi
  
  export MUTILATE_EXPERIMENT_PREFIX='.'
  MUTILATE_BUILD_PATH='../apps/mutilate'
  
  if [ $SERVER_SPEC = 'Linux-10' ]; then
    PREP=prep_linux
    OUTDIR='Linux-10'
    QPS_SWEEP_MAX=1500000
    QPS_SWEEP_NUM_POINTS=30
    MEMCACHED_EXEC='memcached'
    MEMCACHED_BUILD_PATH='../apps/memcached-1.4.18'
    MEMCACHED_BUILD_TARGET=''
    MEMCACHED_PARAMS="-t $MEMCACHED_THREADS -m 8192 -c 65535 -T $MEMCACHED_CORES -u `whoami`"
    MEMCACHED_SHOULD_DEPLOY=1
    IX=0
  elif [ $SERVER_SPEC = 'Linux-40' ]; then
    PREP=prep_linux-40
    OUTDIR='Linux-40'
    QPS_SWEEP_MAX=1500000
    QPS_SWEEP_NUM_POINTS=30
    MEMCACHED_EXEC='memcached'
    MEMCACHED_BUILD_PATH='../apps/memcached-1.4.18'
    MEMCACHED_BUILD_TARGET=''
    MEMCACHED_PARAMS="-t $MEMCACHED_THREADS -m 8192 -c 65535 -T $MEMCACHED_CORES -u `whoami`"
    MEMCACHED_SHOULD_DEPLOY=1
    IX=0
  elif [ $SERVER_SPEC = 'IX-10' ]; then
    PREP=prep_ix
    OUTDIR='IX-10'
    QPS_SWEEP_MAX=1500000
    QPS_SWEEP_NUM_POINTS=30
    MEMCACHED_EXEC=memcached
    MEMCACHED_BUILD_PATH=../apps/memcached-1.4.18-ix
    MEMCACHED_BUILD_TARGET=''
    MEMCACHED_PARAMS='-m 8192 -u `whoami`'
    if [ $CLUSTER_ID = 'EPFL' ]; then
      MEMCACHED_SHOULD_DEPLOY=1
    else
      MEMCACHED_SHOULD_DEPLOY=0
    fi
    IX=1
    IX_PARAMS="-d 0000:42:00.1 -c $MEMCACHED_CORES"
    SERVER_HOST_EXPERIMENT=${SERVER_HOST_EXPERIMENT}:8000
  elif [ $SERVER_SPEC = 'IX-40' ]; then
    PREP=prep_ix-40
    OUTDIR='IX-40'
    QPS_SWEEP_MAX=1500000
    QPS_SWEEP_NUM_POINTS=30
    MEMCACHED_EXEC=memcached
    MEMCACHED_BUILD_PATH=../apps/memcached-1.4.18-ix
    MEMCACHED_BUILD_TARGET=''
    MEMCACHED_PARAMS='-m 8192 -u `whoami`'
    if [ $CLUSTER_ID = 'EPFL' ]; then
      MEMCACHED_SHOULD_DEPLOY=1
    else
      MEMCACHED_SHOULD_DEPLOY=0
    fi
    IX=1
    IX_PARAMS="-d 0000:04:00.0,0000:04:00.1,0000:05:00.0,0000:05:00.1 -c 0,16,2,18,4,20,6,22,8,24,10,26,12,28,14,30"
    SERVER_HOST_EXPERIMENT=${SERVER_HOST_EXPERIMENT}:8000
  else
    echo 'Invalid parameters'
    exit 1
  fi
  
  if [ $MEMCACHED_SHOULD_DEPLOY -eq 1 ]; then
    # Deploy and run memcached
    if [ ! -e $MEMCACHED_BUILD_PATH/Makefile ]; then
      DIR=`pwd`
      cd $MEMCACHED_BUILD_PATH
      cd $DIR
    fi
    
    
    scp $MEMCACHED_BUILD_PATH/$MEMCACHED_EXEC $SERVER_HOST:$MEMCACHED_EXEC
    
    $PREP
    if [ $IX -eq 1 ]; then
      ssh $SERVER_HOST "cd ~/bmos/ix && sudo ./ix $IX_PARAMS -- /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ~/$MEMCACHED_EXEC $MEMCACHED_PARAMS" &
      sleep 10
    else
      MEMCACHED_CMD="sudo nice -n -20 ./$MEMCACHED_EXEC $MEMCACHED_PARAMS"
      ssh $SERVER_HOST $MEMCACHED_CMD &
    fi
  fi
  
  # Deploy mutilate
  cp $MUTILATE_BUILD_PATH/mutilate .
  
  for i in `${AGENT_SUBDIR}/${AGENT_PROFILE}_agentlist.sh`; do
      scp $MUTILATE_BUILD_PATH/mutilate $i:mutilate
  done
  
  # Set up the QPS sweep
  QPS_MIN=$(($QPS_SWEEP_MAX / $QPS_SWEEP_NUM_POINTS))
  QPS_STEP=$(($QPS_SWEEP_MAX / $QPS_SWEEP_NUM_POINTS))
  export MUTILATE_EXTRA_OPTS="--scan=$QPS_MIN:$QPS_SWEEP_MAX:$QPS_STEP"
  
  # Set up the output directory
  OUTDIR_BASE="results/$(date +%Y-%m-%d-%H-%M-%S)"
  OUTDIR=$OUTDIR_BASE/$OUTDIR
  mkdir -p $OUTDIR
  
  # Run the experiments
  experiments
}
