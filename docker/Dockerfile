# Emscripten-based build environment for Poppler + Cairo WASM
FROM emscripten/emsdk:3.1.50

LABEL maintainer="pdf-filler-wasm"
LABEL description="Build environment for compiling Poppler and Cairo to WebAssembly"

# Install build dependencies
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    libtool \
    pkg-config \
    cmake \
    ninja-build \
    meson \
    python3 \
    python3-pip \
    gperf \
    gettext \
    xz-utils \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create directories for dependencies
RUN mkdir -p /opt/wasm-libs/{include,lib,share}

# Set up environment for cross-compilation
ENV WASM_PREFIX=/opt/wasm-libs
ENV PKG_CONFIG_PATH="${WASM_PREFIX}/lib/pkgconfig"
ENV EM_PKG_CONFIG_PATH="${WASM_PREFIX}/lib/pkgconfig"
ENV CFLAGS="-O2 -fPIC"
ENV CXXFLAGS="-O2 -fPIC"

WORKDIR /src
