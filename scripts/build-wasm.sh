#!/bin/bash
set -e

# Build the WASM module linking against compiled dependencies
# This script runs inside the Docker container

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
NATIVE_DIR="${PROJECT_DIR}/native"
DEPS_DIR="${PROJECT_DIR}/deps/install"
BUILD_DIR="${PROJECT_DIR}/build"
DIST_DIR="${PROJECT_DIR}/dist"

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

# Verify dependencies are built
if [ ! -f "${DEPS_DIR}/lib/libpoppler.a" ]; then
    echo "Error: Dependencies not built. Run 'npm run build:deps' first."
    exit 1
fi

echo "=== Building WASM module ==="

# Collect all static libraries (order matters for linking)
LIBS=(
    # Our code depends on poppler
    "${DEPS_DIR}/lib/libpoppler.a"
    "${DEPS_DIR}/lib/libpoppler-cpp.a"
    # Poppler depends on these
    "${DEPS_DIR}/lib/libcairo.a"
    "${DEPS_DIR}/lib/libpixman-1.a"
    "${DEPS_DIR}/lib/libfreetype.a"
    "${DEPS_DIR}/lib/libopenjp2.a"
    "${DEPS_DIR}/lib/libtiff.a"
    "${DEPS_DIR}/lib/libjpeg.a"
    "${DEPS_DIR}/lib/libpng16.a"
    "${DEPS_DIR}/lib/libz.a"
)

# Check for libpng.a vs libpng16.a
if [ -f "${DEPS_DIR}/lib/libpng.a" ] && [ ! -f "${DEPS_DIR}/lib/libpng16.a" ]; then
    # Replace libpng16 with libpng
    LIBS=("${LIBS[@]/libpng16.a/libpng.a}")
fi

# Verify all libraries exist
for lib in "${LIBS[@]}"; do
    if [ ! -f "$lib" ]; then
        echo "Warning: Library not found: $lib"
    else
        echo "Found: $lib"
    fi
done

# Include paths - need both public and private headers for core API
INCLUDES=(
    "-I${DEPS_DIR}/include"
    "-I${DEPS_DIR}/include/poppler"
    "-I${DEPS_DIR}/include/poppler/cpp"
    "-I${DEPS_DIR}/include/poppler/splash"
    "-I${DEPS_DIR}/include/cairo"
    "-I${DEPS_DIR}/include/freetype2"
    "-I${NATIVE_DIR}/include"
)

# We also need the poppler build directory for config headers
POPPLER_BUILD="${PROJECT_DIR}/deps/build/poppler"
if [ -d "${POPPLER_BUILD}" ]; then
    INCLUDES+=("-I${POPPLER_BUILD}")
fi

# Source files
SOURCES=(
    "${NATIVE_DIR}/src/pdf-filler.cpp"
    "${NATIVE_DIR}/src/bindings.cpp"
)

# Emscripten flags
EMFLAGS=(
    "-O2"
    "-std=c++17"
    "-s" "WASM=1"
    "-s" "MODULARIZE=1"
    "-s" "EXPORT_NAME='createPdfFillerModule'"
    "-s" "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','HEAPU8']"
    "-s" "ALLOW_MEMORY_GROWTH=1"
    "-s" "INITIAL_MEMORY=64MB"
    "-s" "MAXIMUM_MEMORY=1GB"
    "-s" "STACK_SIZE=2MB"
    "-s" "NO_EXIT_RUNTIME=1"
    "-s" "FILESYSTEM=1"
    "-s" "FORCE_FILESYSTEM=1"
    "-s" "EXPORTED_FUNCTIONS=['_malloc','_free']"
    "-s" "ENVIRONMENT='web,worker,node'"
    "-s" "SINGLE_FILE=0"
    "-s" "DISABLE_EXCEPTION_CATCHING=0"
    "-lembind"
    "--bind"
)

# Add defines that Poppler needs
DEFINES=(
    "-DPOPPLER_DATADIR=\"/usr/share/poppler\""
)

echo ""
echo "Compiling with:"
echo "  Sources: ${SOURCES[*]}"
echo "  Includes: ${INCLUDES[*]}"
echo ""

# Build the WASM module
em++ \
    "${EMFLAGS[@]}" \
    "${DEFINES[@]}" \
    "${INCLUDES[@]}" \
    "${SOURCES[@]}" \
    "${LIBS[@]}" \
    -o "${DIST_DIR}/pdf-filler.js"

# Check output
if [ -f "${DIST_DIR}/pdf-filler.js" ] && [ -f "${DIST_DIR}/pdf-filler.wasm" ]; then
    # Add ES module exports for browser support
    # Emscripten only generates CommonJS/AMD exports by default
    echo '' >> "${DIST_DIR}/pdf-filler.js"
    echo '// ES Module export for browser support' >> "${DIST_DIR}/pdf-filler.js"
    echo 'export default createPdfFillerModule;' >> "${DIST_DIR}/pdf-filler.js"
    echo 'export { createPdfFillerModule };' >> "${DIST_DIR}/pdf-filler.js"

    JS_SIZE=$(du -h "${DIST_DIR}/pdf-filler.js" | cut -f1)
    WASM_SIZE=$(du -h "${DIST_DIR}/pdf-filler.wasm" | cut -f1)
    echo ""
    echo "=== WASM build complete ==="
    echo "Output:"
    echo "  - ${DIST_DIR}/pdf-filler.js (${JS_SIZE})"
    echo "  - ${DIST_DIR}/pdf-filler.wasm (${WASM_SIZE})"
else
    echo "Error: Build failed - output files not created"
    exit 1
fi
