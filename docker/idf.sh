#!/bin/bash
# idf.py in the build container, e.g. `BOARD=Q1373 ./docker/idf.sh build`.
# The repository is mounted at /home/builder/project, /dev for flashing.

rpath="$( dirname "$( readlink -f "$0" )" )"
tty=""; [ -t 0 ] && tty="-it"

docker run --rm $tty -v /dev:/dev --privileged \
    -e BOARD="${BOARD:-NERDQAXEPLUS2}" -e SDKCONFIG_DEFAULTS \
    -v esp-idf-builder-rust-cargo:/opt/rust/cargo/registry \
    -v "$rpath/..":/home/builder/project \
    "${IMAGE:-shufps/esp-idf-builder-rs:5.3.6}" idf.py "$@"
