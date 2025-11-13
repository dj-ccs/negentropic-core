#!/bin/bash
# build_wasm.sh - WebAssembly Build Script
#
# Compiles negentropic-core to WebAssembly using Emscripten.
#
# Prerequisites:
#   - Emscripten SDK installed and activated
#   - Run: source /path/to/emsdk/emsdk_env.sh
#
# Output:
#   - negentropic_core.wasm: WebAssembly binary
#   - negentropic_core.js: JavaScript wrapper
#
# Usage:
#   ./build/wasm/build_wasm.sh
#
# Author: negentropic-core team
# Version: 0.1.0
# License: MIT OR GPL-3.0

set -e  # Exit on error

# ========================================================================
# CONFIGURATION
# ========================================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/wasm"
OUTPUT_DIR="${BUILD_DIR}/output"

# Source files
SOURCES=(
    "${PROJECT_ROOT}/src/core/state.c"
    "${PROJECT_ROOT}/src/api/negentropic.c"
    "${PROJECT_ROOT}/embedded/se3_math.c"
    "${PROJECT_ROOT}/embedded/trig_tables.c"
    "${PROJECT_ROOT}/embedded/handoff.c"
    "${PROJECT_ROOT}/embedded/t_bsp.c"
)

# Output name
OUTPUT_NAME="negentropic_core"

# Emscripten flags
EMCC_FLAGS=(
    -O3                           # Optimize for speed
    -s WASM=1                     # Generate WASM
    -s STRICT=1                   # Strict mode (determinism)
    -s ALLOW_MEMORY_GROWTH=0      # Fixed memory (determinism)
    -s INITIAL_MEMORY=16MB        # 16MB initial heap
    -s STACK_SIZE=1MB             # 1MB stack
    -s EXPORTED_FUNCTIONS='["_neg_create","_neg_step","_neg_get_state_json","_neg_get_state_binary","_neg_get_state_binary_size","_neg_get_state_hash","_neg_reset_from_binary","_neg_destroy","_neg_get_version","_neg_get_last_error","_neg_get_diagnostics"]'
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]'
    -s MODULARIZE=1               # Export as module
    -s EXPORT_NAME="NegentropicCore"
    --no-entry                    # Library mode
    -I"${PROJECT_ROOT}"           # Include project root
)

# ========================================================================
# CHECKS
# ========================================================================

echo "========================================================================"
echo "WASM Build Script - negentropic-core v0.1.0"
echo "========================================================================"
echo ""

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found. Please install and activate Emscripten SDK."
    echo ""
    echo "Installation:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    echo ""
    exit 1
fi

echo "✓ Emscripten found: $(emcc --version | head -n1)"
echo ""

# ========================================================================
# BUILD
# ========================================================================

echo "Creating output directory..."
mkdir -p "${OUTPUT_DIR}"

echo "Compiling WASM module..."
echo "  Sources: ${#SOURCES[@]} files"
echo "  Output: ${OUTPUT_DIR}/${OUTPUT_NAME}.wasm"
echo ""

# Compile
emcc "${SOURCES[@]}" \
    "${EMCC_FLAGS[@]}" \
    -o "${OUTPUT_DIR}/${OUTPUT_NAME}.js"

# Check output
if [ ! -f "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm" ]; then
    echo "ERROR: WASM file not generated"
    exit 1
fi

if [ ! -f "${OUTPUT_DIR}/${OUTPUT_NAME}.js" ]; then
    echo "ERROR: JS wrapper not generated"
    exit 1
fi

# ========================================================================
# VALIDATION
# ========================================================================

echo "Build successful!"
echo ""
echo "Output files:"
ls -lh "${OUTPUT_DIR}/${OUTPUT_NAME}".{wasm,js}
echo ""

WASM_SIZE=$(wc -c < "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm")
echo "WASM size: $((WASM_SIZE / 1024)) KB"

# ========================================================================
# NODE.JS SMOKE TEST
# ========================================================================

echo ""
echo "Running Node.js smoke test..."

# Create test script
cat > "${OUTPUT_DIR}/test.js" << 'EOF'
const NegentropicCore = require('./negentropic_core.js');

NegentropicCore().then(Module => {
    console.log('  ✓ Module loaded');

    // Get version
    const version = Module.ccall('neg_get_version', 'string', [], []);
    console.log(`  ✓ Version: ${version}`);

    // Create simulation
    const config = JSON.stringify({
        num_entities: 10,
        num_scalar_fields: 100,
        grid_width: 10,
        grid_height: 10,
        grid_depth: 1,
        dt: 0.016,
        precision_mode: 1,
        integrator_type: 0
    });

    const sim = Module.ccall('neg_create', 'number', ['string'], [config]);
    if (sim === 0) {
        console.error('  ✗ Failed to create simulation');
        process.exit(1);
    }
    console.log('  ✓ Simulation created');

    // Step simulation
    const result = Module.ccall('neg_step', 'number', ['number', 'number'], [sim, 0.016]);
    if (result !== 0) {
        console.error('  ✗ Step failed');
        process.exit(1);
    }
    console.log('  ✓ Step executed');

    // Get hash
    const hash = Module.ccall('neg_get_state_hash', 'number', ['number'], [sim]);
    console.log(`  ✓ State hash: 0x${hash.toString(16)}`);

    // Destroy
    Module.ccall('neg_destroy', 'null', ['number'], [sim]);
    console.log('  ✓ Simulation destroyed');

    console.log('');
    console.log('✓ All smoke tests passed!');
}).catch(err => {
    console.error('✗ Error loading module:', err);
    process.exit(1);
});
EOF

# Run Node.js test if available
if command -v node &> /dev/null; then
    cd "${OUTPUT_DIR}"
    if node test.js; then
        echo ""
        echo "✓ Node.js smoke test PASSED"
    else
        echo ""
        echo "✗ Node.js smoke test FAILED"
        exit 1
    fi
else
    echo "  (Node.js not found, skipping smoke test)"
fi

# ========================================================================
# SUMMARY
# ========================================================================

echo ""
echo "========================================================================"
echo "✓ WASM build complete!"
echo "========================================================================"
echo ""
echo "Next steps:"
echo "  1. Test in browser: Serve ${OUTPUT_DIR} via HTTP server"
echo "  2. Integrate with Unity: Import via jslib plugin"
echo "  3. Benchmark: Run performance test (>1000 steps/ms target)"
echo ""
