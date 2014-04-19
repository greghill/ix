#!/bin/sh

set -e

DIR=`dirname $0`
TARGET=$1
PARAMS="-j64 -s"
make -C $DIR/bench    $PARAMS $TARGET
make -C $DIR/dune     $PARAMS $TARGET
make -C $DIR/igb_stub $PARAMS $TARGET
make -C $DIR/ix       $PARAMS $TARGET
make -C $DIR/libix    $PARAMS $TARGET
make -C $DIR/apps/tcp $PARAMS $TARGET
