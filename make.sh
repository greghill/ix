#!/bin/sh

set -e

DIR=`dirname $0`
(cd $DIR/ix && make -j64>/dev/null)
(cd $DIR/libix && make -j64>/dev/null)
(cd $DIR/apps/tcp && make -j64>/dev/null)
(cd $DIR/bench && make -j64>/dev/null)
