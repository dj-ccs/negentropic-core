# Makefile for SE(3) Fixed-Point Tests
# Doom→SE(3) λ-Estimation Service
#
# Usage:
#   make                # Build all tests
#   make test           # Build and run tests
#   make clean          # Remove build artifacts

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../embedded -O2
LDFLAGS = -lm

# Source files
EMBEDDED_DIR = ../embedded
SRC_MATH = $(EMBEDDED_DIR)/se3_math.c
SRC_TRIG = $(EMBEDDED_DIR)/trig_tables.c

# Test executables
TEST_EXEC_MATH = fixed_point_test
TEST_EXEC_TBSP = t_bsp_test

.PHONY: all test test-math test-tbsp clean

all: $(TEST_EXEC_MATH) $(TEST_EXEC_TBSP)

$(TEST_EXEC_MATH): fixed_point_accuracy_test.c $(SRC_MATH) $(SRC_TRIG)
	@echo "Building fixed-point accuracy tests..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Build complete: $(TEST_EXEC_MATH)"

$(TEST_EXEC_TBSP): t_bsp_test.c $(SRC_MATH) $(SRC_TRIG) $(EMBEDDED_DIR)/t_bsp.c $(EMBEDDED_DIR)/handoff.c
	@echo "Building T-BSP tests..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Build complete: $(TEST_EXEC_TBSP)"

test: test-math test-tbsp

test-math: $(TEST_EXEC_MATH)
	@echo ""
	@echo "Running fixed-point math tests..."
	@echo ""
	./$(TEST_EXEC_MATH)

test-tbsp: $(TEST_EXEC_TBSP)
	@echo ""
	@echo "Running T-BSP tests..."
	@echo ""
	./$(TEST_EXEC_TBSP)

clean:
	rm -f $(TEST_EXEC_MATH) $(TEST_EXEC_TBSP)
	@echo "✓ Cleaned build artifacts"

# Help target
help:
	@echo "SE(3) Fixed-Point Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  make        - Build test executable"
	@echo "  make test   - Build and run tests"
	@echo "  make clean  - Remove build artifacts"
	@echo ""
	@echo "Tests verify:"
	@echo "  - Fixed-point arithmetic (FixedMul, FixedDiv)"
	@echo "  - Trigonometric LUT accuracy"
	@echo "  - Rotation matrix operations"
	@echo "  - SE(3) pose transformations"
