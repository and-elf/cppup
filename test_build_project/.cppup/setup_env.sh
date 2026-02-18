#!/bin/bash

# Environment setup script for test_build_project

# Add .cppup/bin to PATH
export PATH="./test_build_project/.cppup/bin:$PATH"

# Set project root
export CPPUP_PROJECT_ROOT="./test_build_project"

echo "Environment set up for test_build_project"
echo "Tools available: $(ls ./test_build_project/.cppup/bin 2>/dev/null || echo 'none')"
