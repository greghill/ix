#!/bin/bash

# Validate arguments
if [ $# -ne 2 ]
then
    echo "Mutilate experiment runner 0.1" >&2
    echo "" >&2
    echo "Usage: $0 memcached_server experiment_name" >&2
    echo "" >&2
    echo "Runs a specified mutilate experiment against a specified memcached instance." >&2
    echo "Produces a csv document to stdout; other messages are to stderr." >&2
    echo "Redirect stdout to a file to get a csv file." >&2
    echo "" >&2
    exit 1
fi

# Ensure the agent swarm is up and running
ps aux | grep `id -un` | grep agents.sh | grep -v grep > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "$0: error: agent swarm is not running!" >&2
    exit 2
fi

# Ensure that the list_agents file can be located
which list_agents.sh > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "$0: error: list_agents.sh could not be found in PATH!" >&2
    exit 3
fi

# Ensure the requested experiment is defined
if [ ! -e $2.experiment ]
then
    echo "$0: error: unknown experiment \"$2\"" >&2
    exit 4
fi

# Run mutilate and produce a CSV file
MUTILATE_OPTS=$(cat $2.experiment | grep -v "#" | tr -s '\n' ' ')
MUTILATE_CMD="mutilate --noload -v -s $1 `list_agents.sh` $MUTILATE_OPTS"
echo "$0: starting experiment" >&2
echo "$0: $MUTILATE_CMD" >&2
$MUTILATE_CMD | tr -s ' ' ','
