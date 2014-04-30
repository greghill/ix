#!/bin/sh

set -e

DIR=`dirname $0`

if [ $# -eq 1 ]; then
  if [ $1 = "1" -o $1 = "0" ]; then
    BUILD_IX=$1
    BENCH_TARGET=
    COMMON_TARGET=
  else
    BUILD_IX=1
    BENCH_TARGET=$1
    COMMON_TARGET=$1
  fi
elif [ $# -eq 2 ]; then
  BUILD_IX=$1
  BENCH_TARGET=$2
  COMMON_TARGET=$2
elif [ $# -ge 3 ]; then
  BUILD_IX=$1
  BENCH_TARGET=$2
  COMMON_TARGET=$3
else
  BUILD_IX=1
  BENCH_TARGET=
  COMMON_TARGET=
fi

PARAMS="-j64 -s"

make -C $DIR/bench      $PARAMS $BENCH_TARGET
if [ $BUILD_IX -eq 1 ]; then
  make -C $DIR/dune     $PARAMS $COMMON_TARGET
  make -C $DIR/igb_stub $PARAMS $COMMON_TARGET
  make -C $DIR/ix       $PARAMS $COMMON_TARGET OSDI_MEMCACHED=1
  make -C $DIR/libix    $PARAMS $COMMON_TARGET OSDI_MEMCACHED=1
  make -C $DIR/apps/event $PARAMS $COMMON_TARGET
fi
