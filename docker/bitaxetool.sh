#!/bin/bash
# bitaxetool in the build container; paths are relative to the repository,
# e.g. `./docker/bitaxetool.sh --config config.cvs --firmware factory.bin -p /dev/ttyACM0`.

rpath="$( dirname "$( readlink -f "$0" )" )"
tty=""; [ -t 0 ] && tty="-it"

docker run --rm $tty -v /dev:/dev --privileged \
    -v "$rpath/..":/home/builder/project \
    "${IMAGE:-shufps/esp-idf-builder-rs:5.3.6}" bitaxetool "$@"
