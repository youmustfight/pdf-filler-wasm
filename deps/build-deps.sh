#!/bin/bash
set -e

# Build dependencies for Poppler + Cairo WASM
# This script runs inside the Docker container

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="${SCRIPT_DIR}"
SRC_DIR="${DEPS_DIR}/src"
BUILD_DIR="${DEPS_DIR}/build"
INSTALL_DIR="${DEPS_DIR}/install"

WASM_PREFIX="${INSTALL_DIR}"
export PKG_CONFIG_PATH="${WASM_PREFIX}/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="${WASM_PREFIX}/lib/pkgconfig"

NPROC=$(nproc)

mkdir -p "${SRC_DIR}" "${BUILD_DIR}" "${INSTALL_DIR}"

# Common emconfigure/emmake wrappers
em_configure() {
    emconfigure ./configure \
        --prefix="${WASM_PREFIX}" \
        --host=wasm32-unknown-emscripten \
        --disable-shared \
        --enable-static \
        "$@"
}

em_cmake() {
    emcmake cmake \
        -DCMAKE_INSTALL_PREFIX="${WASM_PREFIX}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        "$@"
}

download_and_extract() {
    local url="$1"
    local name="$2"
    local dir="${SRC_DIR}/${name}"

    if [ ! -d "${dir}" ] || [ -z "$(ls -A "${dir}" 2>/dev/null)" ]; then
        echo "Downloading ${name}..."
        rm -rf "${dir}"
        mkdir -p "${dir}"

        # Detect compression type from URL
        if [[ "${url}" == *.tar.xz ]]; then
            curl -L "${url}" | tar -xJ -C "${dir}" --strip-components=1
        elif [[ "${url}" == *.tar.gz ]] || [[ "${url}" == *.tgz ]]; then
            curl -L "${url}" | tar -xz -C "${dir}" --strip-components=1
        elif [[ "${url}" == *.tar.bz2 ]]; then
            curl -L "${url}" | tar -xj -C "${dir}" --strip-components=1
        else
            # Default to gzip
            curl -L "${url}" | tar -xz -C "${dir}" --strip-components=1
        fi

        if [ $? -ne 0 ]; then
            echo "Error: Failed to download/extract ${name}"
            exit 1
        fi
    else
        echo "${name} already downloaded"
    fi
}

# ============================================================================
# ZLIB
# ============================================================================
build_zlib() {
    echo "=== Building zlib ==="
    local version="1.3.1"
    download_and_extract \
        "https://zlib.net/zlib-${version}.tar.gz" \
        "zlib"

    cd "${SRC_DIR}/zlib"

    # zlib uses a custom configure
    emconfigure ./configure \
        --prefix="${WASM_PREFIX}" \
        --static

    emmake make -j${NPROC}
    emmake make install

    echo "zlib complete"
}

# ============================================================================
# LIBPNG
# ============================================================================
build_libpng() {
    echo "=== Building libpng ==="
    local version="1.6.40"
    download_and_extract \
        "https://downloads.sourceforge.net/libpng/libpng-${version}.tar.gz" \
        "libpng"

    mkdir -p "${BUILD_DIR}/libpng"
    cd "${BUILD_DIR}/libpng"

    em_cmake "${SRC_DIR}/libpng" \
        -DPNG_SHARED=OFF \
        -DPNG_STATIC=ON \
        -DPNG_TESTS=OFF \
        -DPNG_EXECUTABLES=OFF \
        -DZLIB_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DZLIB_LIBRARY="${WASM_PREFIX}/lib/libz.a"

    emmake make -j${NPROC}
    emmake make install

    echo "libpng complete"
}

# ============================================================================
# FREETYPE
# ============================================================================
build_freetype() {
    echo "=== Building freetype ==="
    local version="2.13.2"
    download_and_extract \
        "https://downloads.sourceforge.net/freetype/freetype-${version}.tar.gz" \
        "freetype"

    mkdir -p "${BUILD_DIR}/freetype"
    cd "${BUILD_DIR}/freetype"

    em_cmake "${SRC_DIR}/freetype" \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_BROTLI=ON \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_PNG=OFF \
        -DFT_DISABLE_ZLIB=OFF \
        -DZLIB_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DZLIB_LIBRARY="${WASM_PREFIX}/lib/libz.a" \
        -DPNG_PNG_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DPNG_LIBRARY="${WASM_PREFIX}/lib/libpng.a"

    emmake make -j${NPROC}
    emmake make install

    echo "freetype complete"
}

# ============================================================================
# PIXMAN
# ============================================================================
build_pixman() {
    echo "=== Building pixman ==="
    local version="0.42.2"
    download_and_extract \
        "https://cairographics.org/releases/pixman-${version}.tar.gz" \
        "pixman"

    mkdir -p "${BUILD_DIR}/pixman"
    cd "${BUILD_DIR}/pixman"

    # Pixman uses meson, but we can use the configure fallback
    cd "${SRC_DIR}/pixman"

    em_configure \
        --disable-gtk \
        --disable-libpng \
        --disable-arm-simd \
        --disable-arm-neon \
        --disable-arm-a64-neon \
        --disable-mmx \
        --disable-sse2 \
        --disable-ssse3 \
        --disable-vmx \
        --disable-openmp

    emmake make -j${NPROC}
    emmake make install

    echo "pixman complete"
}

# ============================================================================
# CAIRO
# ============================================================================
build_cairo() {
    echo "=== Building cairo ==="
    local version="1.18.0"
    download_and_extract \
        "https://cairographics.org/releases/cairo-${version}.tar.xz" \
        "cairo"

    mkdir -p "${BUILD_DIR}/cairo"
    cd "${BUILD_DIR}/cairo"

    # Cairo 1.18+ uses meson
    cat > cross-file.txt << 'MESON_EOF'
[binaries]
c = 'emcc'
cpp = 'em++'
ar = 'emar'
ranlib = 'emranlib'
strip = 'emstrip'
pkgconfig = 'pkg-config'

[host_machine]
system = 'emscripten'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[properties]
needs_exe_wrapper = true

[built-in options]
default_library = 'static'
MESON_EOF

    # Cairo 1.18+ meson options (no win32/quartz/spectre options)
    meson setup "${SRC_DIR}/cairo" \
        --cross-file=cross-file.txt \
        --prefix="${WASM_PREFIX}" \
        --default-library=static \
        -Dfontconfig=disabled \
        -Dfreetype=enabled \
        -Dpng=enabled \
        -Dxlib=disabled \
        -Dxcb=disabled \
        -Dzlib=enabled \
        -Dtests=disabled

    meson compile -j${NPROC}
    meson install

    echo "cairo complete"
}

# ============================================================================
# OPENJPEG (for JPEG2000 support in PDFs)
# ============================================================================
build_openjpeg() {
    echo "=== Building openjpeg ==="
    local version="2.5.0"
    download_and_extract \
        "https://github.com/uclouvain/openjpeg/archive/refs/tags/v${version}.tar.gz" \
        "openjpeg"

    mkdir -p "${BUILD_DIR}/openjpeg"
    cd "${BUILD_DIR}/openjpeg"

    em_cmake "${SRC_DIR}/openjpeg" \
        -DBUILD_CODEC=OFF \
        -DBUILD_TESTING=OFF

    emmake make -j${NPROC}
    emmake make install

    echo "openjpeg complete"
}

# ============================================================================
# LIBJPEG-TURBO
# ============================================================================
build_libjpeg() {
    echo "=== Building libjpeg-turbo ==="
    local version="3.0.1"
    download_and_extract \
        "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/${version}.tar.gz" \
        "libjpeg-turbo"

    mkdir -p "${BUILD_DIR}/libjpeg-turbo"
    cd "${BUILD_DIR}/libjpeg-turbo"

    em_cmake "${SRC_DIR}/libjpeg-turbo" \
        -DWITH_SIMD=OFF \
        -DWITH_TURBOJPEG=OFF \
        -DENABLE_SHARED=OFF

    emmake make -j${NPROC}
    emmake make install

    echo "libjpeg-turbo complete"
}

# ============================================================================
# LIBTIFF
# ============================================================================
build_libtiff() {
    echo "=== Building libtiff ==="
    local version="4.6.0"
    download_and_extract \
        "https://download.osgeo.org/libtiff/tiff-${version}.tar.gz" \
        "libtiff"

    mkdir -p "${BUILD_DIR}/libtiff"
    cd "${BUILD_DIR}/libtiff"

    em_cmake "${SRC_DIR}/libtiff" \
        -Dtiff-tools=OFF \
        -Dtiff-tests=OFF \
        -Dtiff-contrib=OFF \
        -Dtiff-docs=OFF \
        -Djpeg=ON \
        -Dzlib=ON \
        -Dlzma=OFF \
        -Dwebp=OFF \
        -Dzstd=OFF \
        -Djbig=OFF \
        -Dlerc=OFF \
        -DJPEG_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DJPEG_LIBRARY="${WASM_PREFIX}/lib/libjpeg.a" \
        -DZLIB_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DZLIB_LIBRARY="${WASM_PREFIX}/lib/libz.a"

    emmake make -j${NPROC}
    emmake make install

    echo "libtiff complete"
}

# ============================================================================
# BOOST (headers only - needed for poppler)
# Skipping Boost - Poppler can be built with ENABLE_BOOST=OFF
# ============================================================================
build_boost() {
    echo "=== Skipping Boost (not required) ==="
    echo "boost skipped"
}

# ============================================================================
# POPPLER
# ============================================================================
build_poppler() {
    echo "=== Building poppler ==="
    local version="24.01.0"
    download_and_extract \
        "https://poppler.freedesktop.org/poppler-${version}.tar.xz" \
        "poppler"

    # Apply patches if they exist
    if [ -d "${DEPS_DIR}/patches/poppler" ]; then
        cd "${SRC_DIR}/poppler"
        for patch in "${DEPS_DIR}/patches/poppler"/*.patch; do
            [ -f "$patch" ] && patch -p1 < "$patch"
        done
    fi

    mkdir -p "${BUILD_DIR}/poppler"
    cd "${BUILD_DIR}/poppler"

    em_cmake "${SRC_DIR}/poppler" \
        -DENABLE_BOOST=OFF \
        -DENABLE_CPP=ON \
        -DENABLE_UNSTABLE_API_ABI_HEADERS=ON \
        -DENABLE_GLIB=OFF \
        -DENABLE_GOBJECT_INTROSPECTION=OFF \
        -DENABLE_GTK_DOC=OFF \
        -DENABLE_QT5=OFF \
        -DENABLE_QT6=OFF \
        -DENABLE_LIBOPENJPEG=openjpeg2 \
        -DENABLE_LCMS=OFF \
        -DENABLE_DCTDECODER=libjpeg \
        -DENABLE_LIBCURL=OFF \
        -DENABLE_ZLIB=ON \
        -DENABLE_ZLIB_UNCOMPRESS=OFF \
        -DENABLE_SPLASH=ON \
        -DENABLE_UTILS=OFF \
        -DENABLE_NSS3=OFF \
        -DENABLE_GPGME=OFF \
        -DFONT_CONFIGURATION=generic \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_GTK_TESTS=OFF \
        -DBUILD_QT5_TESTS=OFF \
        -DBUILD_QT6_TESTS=OFF \
        -DBUILD_CPP_TESTS=OFF \
        -DBUILD_MANUAL_TESTS=OFF \
        -DFREETYPE_INCLUDE_DIRS="${WASM_PREFIX}/include/freetype2" \
        -DFREETYPE_LIBRARY="${WASM_PREFIX}/lib/libfreetype.a" \
        -DJPEG_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DJPEG_LIBRARY="${WASM_PREFIX}/lib/libjpeg.a" \
        -DOpenJPEG_DIR="${WASM_PREFIX}/lib/openjpeg-2.5" \
        -DTIFF_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DTIFF_LIBRARY="${WASM_PREFIX}/lib/libtiff.a" \
        -DPNG_PNG_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DPNG_LIBRARY="${WASM_PREFIX}/lib/libpng.a" \
        -DZLIB_INCLUDE_DIR="${WASM_PREFIX}/include" \
        -DZLIB_LIBRARY="${WASM_PREFIX}/lib/libz.a" \
        -DCAIRO_INCLUDE_DIRS="${WASM_PREFIX}/include/cairo" \
        -DCAIRO_LIBRARIES="${WASM_PREFIX}/lib/libcairo.a"

    emmake make -j${NPROC}
    emmake make install

    echo "poppler complete"
}

# ============================================================================
# MAIN
# ============================================================================
main() {
    echo "Building dependencies for Poppler WASM..."
    echo "Install prefix: ${WASM_PREFIX}"
    echo ""

    build_zlib
    build_libpng
    build_freetype
    build_pixman
    build_libjpeg
    build_openjpeg
    build_libtiff
    build_cairo
    build_boost
    build_poppler

    echo ""
    echo "=== All dependencies built successfully ==="
    echo "Libraries installed to: ${WASM_PREFIX}"
}

main "$@"
