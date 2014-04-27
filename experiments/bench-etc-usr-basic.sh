#!/bin/bash

experiments() {
  run_experiment fb_etc
  run_experiment fb_usr
}

source $(dirname $0)/bench-common.sh

setup_and_run $@
