#!/bin/bash
# Build script for negentropic-core WASM module

set -e

echo "========================================="
echo "Building negentropic-core WASM Module"
echo "========================================="

# Check if Emscripten is available
if ! command -v emcc &> /dev/null; then
    echo "ERROR: Emscripten not found!"
    echo "Please install Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html"
    echo "Or activate it with: source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

echo "Emscripten version:"
emcc --version

# Create build directory
BUILD_DIR="../build-wasm"
mkdir -p "$BUILD_DIR"

# Configure with Emscripten
echo ""
echo "Configuring CMake with Emscripten..."
emcmake cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WASM=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    ..

# Build
echo ""
echo "Building WASM module..."
cmake --build "$BUILD_DIR" --config Release

# Copy artifacts to web/public/wasm
echo ""
echo "Copying WASM artifacts to web/public/wasm..."
mkdir -p public/wasm
cp "$BUILD_DIR/negentropic_core.js" public/wasm/
cp "$BUILD_DIR/negentropic_core.wasm" public/wasm/
if [ -f "$BUILD_DIR/negentropic_core.worker.js" ]; then
    cp "$BUILD_DIR/negentropic_core.worker.js" public/wasm/
fi
if [ -f "$BUILD_DIR/negentropic_core.d.ts" ]; then
    cp "$BUILD_DIR/negentropic_core.d.ts" src/types/
fi

echo ""
echo "========================================="
echo "WASM build complete!"
echo "========================================="
echo "Artifacts:"
ls -lh public/wasm/

echo ""
echo "You can now run: npm run dev"
