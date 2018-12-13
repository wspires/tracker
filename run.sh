#!/usr/bin/env bash

my_dir=`dirname $0`

# Build and run.
make && ${my_dir}/tracker_test --success

