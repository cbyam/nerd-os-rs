#!/bin/bash
# A shell in the build container (idf.py, esptool.py, cargo, bitaxetool,
# nvs_partition_gen.py). The repository is mounted at /home/builder/project.

rpath="$( dirname "$( readlink -f "$0" )" )"

docker run --rm -it -v /dev:/dev --privileged \
    -e BOARD="${BOARD:-NERDQAXEPLUS2}" -e SDKCONFIG_DEFAULTS \
    -v esp-idf-builder-rust-cargo:/opt/rust/cargo/registry \
    -v "$rpath/..":/home/builder/project \
    "${IMAGE:-shufps/esp-idf-builder-rs:5.3.6}" /bin/bash
