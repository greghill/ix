#!/bin/bash

experiments() {
  run_experiment fb_usr
}

source $(dirname $0)/bench-common.sh

AGENT_PROFILE=osdi14_basic
setup_and_run $@
