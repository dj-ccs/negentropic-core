#!/bin/bash
# build_wasm_benchmark.sh - WASM Physics Integration Benchmark Build
#
# Compiles the physics integration benchmark to WebAssembly for performance testing.
#
# Author: negentropic-core team
# Version: 0.1.0 (PHYS-INT Sprint)
# License: MIT OR GPL-3.0

set -e  # Exit on error

# ========================================================================
# CONFIGURATION
# ========================================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/wasm"
OUTPUT_DIR="${BUILD_DIR}/output"

# Source files - include solvers for physics integration
SOURCES=(
    "${PROJECT_ROOT}/tests/physics_integration_benchmark.c"
    "${PROJECT_ROOT}/src/solvers/hydrology_richards_lite.c"
    "${PROJECT_ROOT}/src/solvers/regeneration_cascade.c"
    "${PROJECT_ROOT}/src/solvers/regeneration_microbial.c"
)

# Output name
OUTPUT_NAME="physics_benchmark"

# Emscripten flags for standalone WASM executable
EMCC_FLAGS=(
    -O3                                    # Optimize for speed (same as native)
    -s WASM=1                              # Generate WASM
    -s STANDALONE_WASM                     # Standalone executable mode
    -s INITIAL_MEMORY=32MB                 # 32MB for grid + solvers
    -s STACK_SIZE=2MB                      # 2MB stack
    -s ALLOW_MEMORY_GROWTH=0               # Fixed memory for determinism
    -s TOTAL_STACK=2MB                     # Match stack size
    -I"${PROJECT_ROOT}"                    # Include project root
    -I"${PROJECT_ROOT}/src/solvers"        # Include solver headers
)

# ========================================================================
# CHECKS
# ========================================================================

echo "========================================================================"
echo "WASM Physics Integration Benchmark Build"
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

echo "Compiling WASM benchmark..."
echo "  Sources: ${#SOURCES[@]} files"
echo "  Output: ${OUTPUT_DIR}/${OUTPUT_NAME}.wasm"
echo ""

# Compile
emcc "${SOURCES[@]}" \
    "${EMCC_FLAGS[@]}" \
    -o "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm"

# Check output
if [ ! -f "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm" ]; then
    echo "ERROR: WASM file not generated"
    exit 1
fi

# ========================================================================
# VALIDATION
# ========================================================================

echo ""
echo "Build successful!"
echo ""
echo "Output file:"
ls -lh "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm"
echo ""

WASM_SIZE=$(wc -c < "${OUTPUT_DIR}/${OUTPUT_NAME}.wasm")
echo "WASM size: $((WASM_SIZE / 1024)) KB"

# ========================================================================
# NODE.JS BENCHMARK TEST
# ========================================================================

echo ""
echo "Running WASM benchmark via Node.js..."
echo ""

# Check Node.js availability
if ! command -v node &> /dev/null; then
    echo "WARNING: Node.js not found. Cannot run WASM benchmark."
    echo "Install Node.js to test WASM performance."
    exit 0
fi

# Run WASM benchmark
cd "${OUTPUT_DIR}"
if node "${OUTPUT_NAME}.wasm"; then
    echo ""
    echo "✓ WASM benchmark completed successfully"
else
    echo ""
    echo "✗ WASM benchmark failed"
    exit 1
fi

# ========================================================================
# SUMMARY
# ========================================================================

echo ""
echo "========================================================================"
echo "✓ WASM BENCHMARK BUILD COMPLETE"
echo "========================================================================"
echo ""
echo "Performance comparison:"
echo "  1. Native C: Run ./tests/physics_integration_benchmark"
echo "  2. WASM: Run node build/wasm/output/physics_benchmark.wasm"
echo ""
echo "Critical metric: ns/cell/step"
echo "  - < 150 ns: EXCELLENT, proceed to GEO-v1"
echo "  - 150-250 ns: ACCEPTABLE, plan Doom Engine optimization"
echo "  - > 250 ns: PROBLEM, halt visualization work"
echo ""
