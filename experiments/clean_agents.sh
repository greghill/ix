#!/bin/bash

# Validate arguments
if [ $# -ne 1 ]
then
    echo "Mutilate experiment agent cleaner 0.4" >&2
    echo "" >&2
    echo "Usage: $0 experiment_name" >&2
    echo "" >&2
    echo "Cleans all the agent processes created by a specified experiment." >&2
    exit 1
fi

# Ensure the requested experiment is defined
if [ ! -e $(dirname $0)/$1.experiment ]
then
    echo "$0: error: unknown experiment \"$1\"" >&2
    exit 1
fi

# Ensure that the experiment specifies an agent profile
cat $(dirname $0)/$1.experiment | grep "^AGENTS:" > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "$0: error: experiment \"$1\" does not specify an agent profile" >&2
    exit 2
fi

# Extract the agent profile from the experiment definition
AGENT_PROFILE=$(cat $(dirname $0)/$1.experiment | grep "^AGENTS:" | sed s/AGENTS://)
AGENT_LIST="$(dirname $0)/${AGENT_SUBDIR}${AGENT_PROFILE}_agentlist.sh"

# Ensure the agent profile exists
if [ ! -e $AGENT_LIST ]
then
    echo "$0: error: unknown agent profile \"$AGENT_PROFILE\"" >&2
    exit 3
fi

# Clean the agents by SSHing to the server and killing the process
for i in `$AGENT_LIST`
do
    echo "$0: killing agent on $i" >&2
    ssh $i killall mutilate
done
