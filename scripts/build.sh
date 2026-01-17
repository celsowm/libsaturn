#!/bin/bash
set -e

echo "Building libsaturn..."
make clean
make lib

echo ""
echo "Building examples..."
make examples

echo ""
echo "Build complete!"
echo ""
echo "Library: lib/libsaturn.a"
echo "Examples:"
find examples -name "0.BIN" -exec echo "  - {}" \;
