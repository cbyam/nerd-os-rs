# The build image of the firmware (published as shufps/esp-idf-builder-rs,
# also used by CI): ESP-IDF 5.3.6, Node.js 20 for the web UI, bitaxetool and
# nvs_partition_gen.py, and the Rust Xtensa toolchain ("esp") for
# components/rust_core.
FROM espressif/idf:v5.3.6 AS full

USER root

RUN apt-get update && \
    apt-get install -y curl python3-pip git vim && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# An unprivileged user with UID:GID 1000:1000
RUN groupadd -g 1000 builder && \
    useradd -m -u 1000 -g builder -s /bin/bash builder

RUN pip3 install bitaxetool && \
    git clone --depth 1 https://github.com/espressif/esp-idf-nvs-partition-gen.git /tmp/nvs-gen && \
    cp /tmp/nvs-gen/esp_idf_nvs_partition_gen/nvs_partition_gen.py /usr/local/bin/ && \
    chmod +x /usr/local/bin/nvs_partition_gen.py && \
    rm -rf /tmp/nvs-gen

# Installed to /opt/rust so it works for both the builder user and --user root (CI).
# The scripts mount a named volume on /opt/rust/cargo/registry to cache crates.
ARG ESPUP_VERSION=0.12.2
ARG ESP_RUST_VERSION=1.80.0.0
ENV RUSTUP_HOME=/opt/rust/rustup \
    CARGO_HOME=/opt/rust/cargo \
    PATH=/opt/rust/cargo/bin:$PATH
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain none && \
    curl -sSfL -o /usr/local/bin/espup \
        https://github.com/esp-rs/espup/releases/download/v${ESPUP_VERSION}/espup-x86_64-unknown-linux-gnu && \
    chmod +x /usr/local/bin/espup && \
    espup install --targets esp32s3 --toolchain-version ${ESP_RUST_VERSION} \
        --export-file /opt/rust/export-esp.sh && \
    mkdir -p /opt/rust/cargo/registry && \
    chown -R builder:builder /opt/rust

# bindgen (the IDF bindings of rust_core) needs a libclang that knows the
# Xtensa target: espup's esp-clang. Only the library and clang's builtin
# headers are kept, at a fixed path (LIBCLANG_PATH below).
RUN CLANG_LIB=$(dirname $(find /opt/rust/rustup/toolchains/esp/xtensa-esp32-elf-clang -name 'libclang.so*' | head -n1)) && \
    mkdir -p /opt/rust/libclang && \
    cp -a ${CLANG_LIB}/libclang.so* /opt/rust/libclang/ && \
    cp -a ${CLANG_LIB}/clang /opt/rust/libclang/clang && \
    ls /opt/rust/libclang

# Not needed for the build, removed to keep the image small (CI runner disk):
# - the ESP-IDF git history (3.5 GB); IDF_VER then comes from version.txt,
# - espup's own GCC and the rest of its clang: rustc links with ESP-IDF's
#   GCC, bindgen only needs libclang (kept above).
RUN git -C /opt/esp/idf describe --tags --dirty > /opt/esp/idf/version.txt && \
    rm -rf /opt/esp/idf/.git \
        /opt/rust/rustup/toolchains/esp/xtensa-esp-elf \
        /opt/rust/rustup/toolchains/esp/xtensa-esp32-elf-clang \
        /opt/rust/export-esp.sh && \
    chown -R builder:builder /opt/rust/libclang

# Squashed: files deleted above would otherwise stay in the lower layers.
FROM scratch
COPY --from=full / /
ENV PATH=/opt/rust/cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    IDF_PATH=/opt/esp/idf \
    IDF_TOOLS_PATH=/opt/esp \
    IDF_GITHUB_ASSETS= \
    IDF_PYTHON_CHECK_CONSTRAINTS=no \
    IDF_CCACHE_ENABLE=1 \
    RUSTUP_HOME=/opt/rust/rustup \
    CARGO_HOME=/opt/rust/cargo \
    LIBCLANG_PATH=/opt/rust/libclang
USER builder
WORKDIR /home/builder/project
ENTRYPOINT ["/opt/esp/entrypoint.sh"]
CMD ["/bin/bash"]
