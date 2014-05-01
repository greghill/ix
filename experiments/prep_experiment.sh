#!/bin/bash

# Validate arguments
if [ $# -ne 2 ]
then
    echo "Mutilate experiment preparer 0.4" >&2
    echo "" >&2
    echo "Usage: $0 memcached_server experiment_name" >&2
    echo "" >&2
    echo "Preps a specified mutilate experiment against a specified memcached instance." >&2
    echo "This involves loading the memcached database with experiment keys and values." >&2
    echo "" >&2
    exit 1
fi

# Ensure the requested experiment is defined
if [ ! -e $(dirname $0)/$2.experiment ]
then
    echo "$0: error: unknown experiment \"$2\"" >&2
    exit 1
fi

# Check if the MUTILATE_EXPERIMENT_PREFIX variable set and warn if not
if [ "$(echo $MUTILATE_EXPERIMENT_PREFIX)" == "" ]
then
    echo "$0: warning: MUTILATE_EXPERIMENT_PREFIX not set, using PATH for mutilate" >&2
fi

# Run mutilate and prep the experiment
MUTILATE_OPTS=$(cat $(dirname $0)/$2.experiment | grep -- -[KVr] | tr -s '\n' ' ')
MUTILATE_CMD="${MUTILATE_EXPERIMENT_PREFIX}/mutilate --loadonly --binary -v -s $1 $MUTILATE_AGENTS $MUTILATE_OPTS"
echo "$0: preparing experiment" >&2
echo "$0: $MUTILATE_CMD" >&2
$MUTILATE_CMD
