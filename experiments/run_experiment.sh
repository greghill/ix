#!/bin/bash

# Validate arguments
if [ $# -ne 2 ]
then
    echo "Mutilate experiment runner 0.3" >&2
    echo "" >&2
    echo "Usage: $0 memcached_server experiment_name" >&2
    echo "" >&2
    echo "Runs a specified mutilate experiment against a specified memcached instance." >&2
    echo "Produces a csv document to stdout; other messages are to stderr." >&2
    echo "Redirect stdout to a file to get a csv file." >&2
    echo "" >&2
    exit 1
fi

# Collect arguments
MEMCACHED_SERVER=$1
EXPERIMENT_NAME=$2

# Ensure the requested experiment is defined
if [ ! -e $(dirname $0)/$EXPERIMENT_NAME.experiment ]
then
    echo "$0: error: unknown experiment \"$EXPERIMENT_NAME\"" >&2
    exit 1
fi

# Ensure that the experiment specifies an agent profile
cat $(dirname $0)/$EXPERIMENT_NAME.experiment | grep "^AGENTS:" > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "$0: error: experiment \"$EXPERIMENT_NAME\" does not specify an agent profile" >&2
    exit 2
fi

# Check if the AGENT_SUBDIR variable is set and warn if not
if [ "$(echo $AGENT_SUBDIR)" == "" ]
then
    echo "$0: warning: AGENT_SUBDIR not set, using \"$(dirname $0)\" for agent profiles" >&2
fi

# Extract the agent profile from the experiment definition
AGENT_PROFILE=$(cat $(dirname $0)/$EXPERIMENT_NAME.experiment | grep "^AGENTS:" | sed s/AGENTS://)
AGENT_LIST="$(dirname $0)/${AGENT_SUBDIR}${AGENT_PROFILE}_agentlist.sh"
AGENT_RUN="$(dirname $0)/${AGENT_SUBDIR}${AGENT_PROFILE}_agentrun.sh"

# Ensure the agent profile exists
if [ ! -e $AGENT_LIST ]
then
    echo "$0: error: unknown agent profile \"$AGENT_PROFILE\"" >&2
    exit 3
fi

# Check if the MUTILATE_EXPERIMENT_PREFIX variable set and warn if not
if [ "$(echo $MUTILATE_EXPERIMENT_PREFIX)" == "" ]
then
    echo "$0: warning: MUTILATE_EXPERIMENT_PREFIX not set, using PATH for mutilate" >&2
fi

# Build the command string for starting the agents and make sure it worked
# Do this only if the agent profile specifies that agents should be started
if [ -e $AGENT_RUN ]
then
    for i in `$AGENT_LIST`
    do
        AGENT_STRING+=("$($AGENT_RUN $i)")
    done
else
    echo "$0: warning: agent profile \"$AGENT_PROFILE\" assumes agents started manually" >&2
fi

if [ $? -ne 0 ]
then
    echo "$0: error: failed to create command string for agent profile \"$AGENT_PROFILE\"" >&2
    exit 4
fi

# Start the agents, if the profile says to do so
if [ -e $AGENT_RUN ]
then
    $(dirname $0)/swarm -w "${AGENT_STRING[@]}" &
    SWARM_PID=$!
    trap "kill $SWARM_PID > /dev/null 2>&1" EXIT SIGHUP SIGINT SIGQUIT SIGTERM
    echo "$0: started agents for profile \"$AGENT_PROFILE\"" >&2
fi

# Run mutilate and produce a CSV file
MUTILATE_OPTS=$(cat $(dirname $0)/$EXPERIMENT_NAME.experiment | grep -v "#" | grep -v "AGENTS:" | tr -s '\n' ' ')

for i in `$AGENT_LIST`
do
    MUTILATE_AGENTS+="-a $i "
done

MUTILATE_CMD="nice -n -20 ${MUTILATE_EXPERIMENT_PREFIX}/mutilate --noload --binary -v -s $MEMCACHED_SERVER $MUTILATE_AGENTS $MUTILATE_OPTS ${MUTILATE_EXTRA_OPTS}"
echo "$0: starting experiment" >&2
echo "$0: $MUTILATE_CMD" >&2
sudo $MUTILATE_CMD | tr -s ' ' ','
