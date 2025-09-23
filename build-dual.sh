#!/bin/bash
# build-dual.sh - Dual build system for glib.wasm
#
# Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
# Copyright (c) 2025 Superstruct Ltd, New Zealand
# Licensed under LGPL-2.1-or-later

set -euo pipefail

VARIANT="${1:-all}"
BUILD_DIR_BASE="$(pwd)/build-dual"
INSTALL_PREFIX="$(pwd)/install"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check prerequisites
check_prerequisites() {
    log_info "Checking build prerequisites..."

    if ! command -v emcc &> /dev/null; then
        log_error "Emscripten not found. Please install and activate EMSDK."
        exit 1
    fi

    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found. Please install CMake."
        exit 1
    fi

    # Check for PCRE2 SIDE_MODULE dependency
    if [ -f "../pcre2.wasm/install/wasm/pcre2-side.wasm" ]; then
        log_info "PCRE2 SIDE_MODULE found - regex support enabled"
        export PCRE2_AVAILABLE=1
    else
        log_warning "PCRE2 SIDE_MODULE not found - using internal regex"
        export PCRE2_AVAILABLE=0
    fi

    # Check for libffi SIDE_MODULE dependency
    if [ -f "../libffi.wasm/install/wasm/libffi-side.wasm" ]; then
        log_info "libffi SIDE_MODULE found - GObject support enabled"
        export LIBFFI_AVAILABLE=1
    else
        log_warning "libffi SIDE_MODULE not found - GObject support disabled"
        export LIBFFI_AVAILABLE=0
    fi

    log_success "Prerequisites check completed"
}

# Clean build directories
clean_build() {
    log_info "Cleaning build artifacts..."
    rm -rf "${BUILD_DIR_BASE}-side" "${BUILD_DIR_BASE}-main" "${INSTALL_PREFIX}"
    rm -rf build-side build-main build
    log_success "Build artifacts cleaned"
}

# Build SIDE_MODULE (production)
build_side_module() {
    log_info "Building glib-side.wasm for production..."

    BUILD_DIR="${BUILD_DIR_BASE}-side"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # Configure with CMake for SIDE_MODULE
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SIDE_MODULE=ON \
        -DBUILD_MAIN_MODULE=OFF \
        -DENABLE_SIMD=ON \
        -DENABLE_THREADING=ON \
        -DENABLE_OPFS=ON \
        -DENABLE_BROWSER_MAINLOOP=ON \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

    # Build the SIDE_MODULE
    emmake make -j$(nproc)

    # Install artifacts
    make install

    # Copy SIDE_MODULE to standard location
    mkdir -p "${INSTALL_PREFIX}/wasm"
    cp glib-side.wasm "${INSTALL_PREFIX}/wasm/"

    log_success "SIDE_MODULE: ${INSTALL_PREFIX}/wasm/glib-side.wasm"
    ls -lh "${INSTALL_PREFIX}/wasm/glib-side.wasm"
    cd ..
}

# Build MAIN_MODULE (testing/NPM)
build_main_module() {
    log_info "Building glib-main.js for testing..."

    BUILD_DIR="${BUILD_DIR_BASE}-main"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # First, use CMake to compile static library with all GLib sources
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SIDE_MODULE=OFF \
        -DBUILD_MAIN_MODULE=OFF \
        -DENABLE_SIMD=ON \
        -DENABLE_THREADING=ON \
        -DENABLE_OPFS=OFF \
        -DENABLE_BROWSER_MAINLOOP=OFF \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

    # Build static library only
    emmake make glib-wasm -j$(nproc)

    # Now use direct emcc to build MAIN_MODULE with correct flags (like zlib.wasm)
    log_info "Compiling MAIN_MODULE with direct emcc..."

    # GLib WASM wrapper + static library - DIRECT EMCC APPROACH
    emcc ../wasm/glib_wasm_wrapper.c ../wasm/wasm_compat.c \
        ../wasm/web_native_capabilities.c \
        ../wasm/web_native_simd_*.c \
        ../wasm/web_native_crypto.c \
        ../wasm/web_native_networking.c \
        ../wasm/web_native_memory.c \
        ../wasm/web_native_filesystem.c \
        -L. -lglib-wasm \
        -I.. -I../glib -I. \
        -DGLIB_STATIC_COMPILATION=1 \
        -DGLIB_WASM_NATIVE=1 \
        -DGLIB_COMPILATION=1 \
        -include config.h \
        -O3 \
        -flto \
        -msimd128 \
        -sWASM=1 \
        -sMODULARIZE=1 \
        -sEXPORT_ES6=1 \
        -sEXPORT_NAME="createModule" \
        -sEXPORTED_FUNCTIONS='["_glib_wasm_init","_glib_wasm_get_version","_glib_wasm_get_build_info","_glib_wasm_test_basic","_glib_wasm_test_filesystem","_glib_wasm_test_simd","_glib_wasm_benchmark_simd","_glib_wasm_cleanup","_malloc","_free"]' \
        -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8","HEAPU32","HEAP32","UTF8ToString","stringToUTF8","getValue","setValue"]' \
        -sINITIAL_MEMORY=67108864 \
        -sMAXIMUM_MEMORY=536870912 \
        -sALLOW_MEMORY_GROWTH=1 \
        -sSTACK_SIZE=5242880 \
        -sASSERTIONS=0 \
        -sNO_EXIT_RUNTIME=1 \
        -o glib-release.js

    # Copy MAIN_MODULE files to standard location
    mkdir -p "${INSTALL_PREFIX}/wasm"
    if [ -f "glib-release.js" ]; then
        cp "glib-release.js" "${INSTALL_PREFIX}/wasm/"
        cp "glib-release.wasm" "${INSTALL_PREFIX}/wasm/"
        log_success "MAIN_MODULE: ${INSTALL_PREFIX}/wasm/glib-release.js"
        ls -lh "${INSTALL_PREFIX}/wasm/glib-release.js" "${INSTALL_PREFIX}/wasm/glib-release.wasm"
    else
        log_error "MAIN_MODULE build failed - glib-release.js not found"
        exit 1
    fi
    cd ..
}

# Display build summary
build_summary() {
    log_info "Build Summary:"
    echo "==============================================="

    if [ -f "${INSTALL_PREFIX}/wasm/glib-side.wasm" ]; then
        SIDE_SIZE=$(stat -f%z "${INSTALL_PREFIX}/wasm/glib-side.wasm" 2>/dev/null || stat -c%s "${INSTALL_PREFIX}/wasm/glib-side.wasm")
        echo "SIDE_MODULE: $(numfmt --to=iec ${SIDE_SIZE}) (${INSTALL_PREFIX}/wasm/glib-side.wasm)"
    fi

    if [ -f "${INSTALL_PREFIX}/wasm/glib-release.js" ]; then
        MAIN_JS_SIZE=$(stat -f%z "${INSTALL_PREFIX}/wasm/glib-release.js" 2>/dev/null || stat -c%s "${INSTALL_PREFIX}/wasm/glib-release.js")
        MAIN_WASM_SIZE=$(stat -f%z "${INSTALL_PREFIX}/wasm/glib-release.wasm" 2>/dev/null || stat -c%s "${INSTALL_PREFIX}/wasm/glib-release.wasm")
        echo "MAIN_MODULE: $(numfmt --to=iec ${MAIN_JS_SIZE}) JS + $(numfmt --to=iec ${MAIN_WASM_SIZE}) WASM"
    fi

    echo "==============================================="
    echo "Dependencies:"
    echo "- PCRE2: ${PCRE2_AVAILABLE:-0} (regex support)"
    echo "- libffi: ${LIBFFI_AVAILABLE:-0} (GObject support)"
    echo "==============================================="
}

case "$VARIANT" in
    side)
        check_prerequisites
        build_side_module
        build_summary
        ;;
    main)
        check_prerequisites
        build_main_module
        build_summary
        ;;
    all)
        check_prerequisites
        build_side_module
        build_main_module
        build_summary
        ;;
    clean)
        clean_build
        ;;
    *)
        echo "Usage: $0 [side|main|all|clean]"
        echo ""
        echo "  side  - Build SIDE_MODULE for dynamic linking (production)"
        echo "  main  - Build MAIN_MODULE for testing and NPM"
        echo "  all   - Build both SIDE_MODULE and MAIN_MODULE"
        echo "  clean - Clean all build artifacts"
        exit 1
        ;;
esac

log_success "Build completed successfully!"