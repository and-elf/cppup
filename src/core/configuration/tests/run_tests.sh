#!/bin/bash

echo "Building and running configuration API tests..."
echo

# Set compiler flags
CXX_FLAGS="-std=c++20 -Wall -Wextra -I.."

# Test types
echo "=== Testing basic types ==="
if g++ $CXX_FLAGS test_types.cpp -o test_types && ./test_types; then
    echo "✓ Types test passed"
else
    echo "✗ Types test failed"
    exit 1
fi

echo

# Test outputs
echo "=== Testing output types ==="
if g++ $CXX_FLAGS test_outputs.cpp -o test_outputs && ./test_outputs; then
    echo "✓ Outputs test passed"
else
    echo "✗ Outputs test failed"
    exit 1
fi

echo

# Test profile
echo "=== Testing profile ==="
if g++ $CXX_FLAGS test_profile.cpp -o test_profile && ./test_profile; then
    echo "✓ Profile test passed"
else
    echo "✗ Profile test failed"
    exit 1
fi

echo

# Test compile_commands emitter
echo "=== Testing compile_commands emitter ==="
if g++ -std=c++23 -Wall -Wextra -I.. \
       test_compile_commands.cpp ../compile_commands.cpp \
       -o test_compile_commands && ./test_compile_commands; then
    echo "✓ compile_commands test passed"
else
    echo "✗ compile_commands test failed"
    exit 1
fi

echo
echo "=== All tests passed! ==="

# Clean up
rm -f test_types test_outputs test_profile test_compile_commands