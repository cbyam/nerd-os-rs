#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

docker run --rm -it -v /dev:/dev --privileged  -e BOARD="${BOARD:-NERDQAXEPLUS2}" -v esp-idf-builder-rust-cargo:/opt/rust/cargo/registry -v "$rpath/..":/home/builder/project esp-idf-builder-rust idf.py $@
