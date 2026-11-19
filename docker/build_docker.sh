#!/bin/bash
# Builds the image locally under the name the scripts use by default
# (instead of pulling it from Docker Hub).

rpath="$( dirname "$( readlink -f "$0" )" )"
cd "$rpath"

docker build -t "${IMAGE:-shufps/esp-idf-builder-rs:5.3.6}" .
