#!/bin/bash

docker run --rm -it \
    -v "$(pwd)":/home/builder/project \
    -v esp-idf-builder-rust-cargo:/opt/rust/cargo/registry \
    -w /home/builder/project \
    esp-idf-builder-rust:latest \
    idf.py "$@"
