#!/bin/bash
set -euo pipefail

# GLib WASM Unified Build System
# Follows GTK.wasm pattern with Meson as source of truth

BUILD_TYPE="${1:-standard}"
CLEAN="${CLEAN:-false}"
FETCH_ONLY="${FETCH_ONLY:-false}"

echo "🔨 GLib.wasm Unified Build System"
echo "Build Type: $BUILD_TYPE"
echo "Clean: $CLEAN"
echo "Fetch Only: $FETCH_ONLY"
echo ""

# 1. Validate tools
echo "🔍 Validating build tools..."
command -v emcc >/dev/null 2>&1 || { echo "❌ emcc not found - install Emscripten SDK"; exit 1; }
command -v meson >/dev/null 2>&1 || { echo "❌ meson not found - install Meson build system"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "❌ ninja not found - install Ninja build system"; exit 1; }

EMCC_VERSION=$(emcc --version | head -1)
MESON_VERSION=$(meson --version)
echo "✅ emcc: $EMCC_VERSION"
echo "✅ meson: $MESON_VERSION"
echo ""

# 2. Clean if requested
if [[ "$CLEAN" == "true" ]]; then
  echo "🧹 Cleaning build artifacts..."
  rm -rf build build-* install
  echo "✅ Cleaned build artifacts"
  echo ""
fi

# 3. Fetch or build dependencies
if [[ -f "dependencies.json" ]]; then
  echo "📦 Fetching dependencies..."
  if [[ -f "scripts/fetch-dependencies.sh" ]]; then
    bash scripts/fetch-dependencies.sh || echo "⚠️  Dependency fetch failed, will build locally"
  else
    echo "⚠️  scripts/fetch-dependencies.sh not found, skipping dependency fetch"
  fi
  echo ""
fi

if [[ "$FETCH_ONLY" == "true" ]]; then
  echo "✅ Dependencies fetched (fetch-only mode)"
  exit 0
fi

# 4. Set build type options
case "$BUILD_TYPE" in
  minimal)
    WASM_BUILD_TYPE="minimal"
    WASM_OPTIMIZE="size"
    WASM_SIMD="false"
    WASM_THREADING="false"
    WASM_WEBGPU="false"
    echo "📦 Minimal build: Size-optimized, no SIMD/threading"
    ;;
  webgpu)
    WASM_BUILD_TYPE="webgpu"
    WASM_OPTIMIZE="speed"
    WASM_SIMD="true"
    WASM_THREADING="true"
    WASM_WEBGPU="true"
    echo "🎮 WebGPU build: GPU-accelerated, SIMD, threading"
    ;;
  standard|*)
    WASM_BUILD_TYPE="standard"
    WASM_OPTIMIZE="balanced"
    WASM_SIMD="true"
    WASM_THREADING="true"
    WASM_WEBGPU="false"
    echo "⚡ Standard build: Balanced, SIMD, threading"
    ;;
esac
echo ""

# 5. Check for libffi dependency (required for GLib)
if [[ -d "../libffi.wasm/install/wasm/pkgconfig" ]]; then
  export PKG_CONFIG_PATH="${PWD}/../libffi.wasm/install/wasm/pkgconfig:${PKG_CONFIG_PATH:-}"
  echo "✅ Found libffi.wasm dependency"
else
  echo "⚠️  libffi.wasm not found at ../libffi.wasm - may need to build it first"
fi
echo ""

# 6. Configure Meson
echo "⚙️  Configuring Meson build system..."
BUILD_DIR="build-${BUILD_TYPE}"

meson setup "$BUILD_DIR" \
  --cross-file=scripts/emscripten.cross \
  --prefix="$(pwd)/install" \
  --libdir=wasm \
  --bindir=wasm \
  -Ddefault_library=static \
  -Dxattr=false \
  -Dtests=false \
  -Dintrospection=disabled \
  -Dnls=disabled \
  --force-fallback-for=libpcre2-8 \
  -Dbuildtype=release \
  -Dwasm_build_type="$WASM_BUILD_TYPE" \
  -Dwasm_simd="$WASM_SIMD" \
  -Dwasm_threading="$WASM_THREADING" \
  -Dwasm_webgpu="$WASM_WEBGPU" \
  -Dwasm_optimize="$WASM_OPTIMIZE" \
  --reconfigure

echo "✅ Meson configured"
echo ""

# 7. Build
echo "🔨 Building GLib WASM..."
meson compile -C "$BUILD_DIR"
echo "✅ Build complete"
echo ""

# 8. Install
echo "📦 Installing to install/wasm/..."
meson install -C "$BUILD_DIR"
echo "✅ Install complete"
echo ""

# 9. Rename .so to .wasm for SIDE_MODULE
if [[ -f "install/wasm/glib-side.so" ]]; then
  mv install/wasm/glib-side.so install/wasm/glib-side.wasm
  echo "✅ Renamed glib-side.so to glib-side.wasm"
fi

# 10. Post-process with wasm-opt (if available)
if command -v wasm-opt >/dev/null 2>&1; then
  echo "🔧 Optimizing with wasm-opt..."
  for wasm_file in install/wasm/*.wasm; do
    if [[ -f "$wasm_file" ]]; then
      wasm-opt -O3 "$wasm_file" -o "$wasm_file.opt"
      mv "$wasm_file.opt" "$wasm_file"
      echo "✅ Optimized $(basename $wasm_file)"
    fi
  done
  echo ""
else
  echo "⚠️  wasm-opt not found - skipping optimization (install binaryen for smaller binaries)"
  echo ""
fi

# 11. Generate manifest
echo "📋 Generating manifest..."
GLIB_VERSION=$(git describe --tags --always 2>/dev/null || echo "2.78.1")
BUILD_DATE=$(date -u +%Y-%m-%dT%H:%M:%SZ)

cat > install/wasm/manifest.json <<EOF
{
  "library": "glib.wasm",
  "version": "$GLIB_VERSION",
  "buildType": "$BUILD_TYPE",
  "timestamp": "$BUILD_DATE",
  "features": {
    "simd": $WASM_SIMD,
    "threading": $WASM_THREADING,
    "webgpu": $WASM_WEBGPU
  },
  "files": $(cd install/wasm && ls -1 *.wasm *.js 2>/dev/null | jq -R . | jq -s . || echo '[]')
}
EOF
echo "✅ Manifest generated"
echo ""

# 12. Display results
echo "📊 Build Results:"
echo "=" "$(printf '=%.0s' {1..60})"
ls -lh install/wasm/
echo ""

if [[ -f "install/wasm/glib-side.wasm" ]]; then
  SIDE_SIZE=$(stat -c%s "install/wasm/glib-side.wasm" 2>/dev/null || stat -f%z "install/wasm/glib-side.wasm" 2>/dev/null || echo "0")
  SIDE_SIZE_KB=$((SIDE_SIZE / 1024))
  echo "✅ SIDE_MODULE: glib-side.wasm ($SIDE_SIZE_KB KB)"
fi

if [[ -f "install/wasm/glib-main.wasm" ]]; then
  MAIN_SIZE=$(stat -c%s "install/wasm/glib-main.wasm" 2>/dev/null || stat -f%z "install/wasm/glib-main.wasm" 2>/dev/null || echo "0")
  MAIN_SIZE_KB=$((MAIN_SIZE / 1024))
  echo "✅ MAIN_MODULE: glib-main.wasm ($MAIN_SIZE_KB KB)"
fi

echo ""
echo "🎉 Build complete! Output in install/wasm/"
