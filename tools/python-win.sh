#!/bin/bash
# Wrapper to use Windows Python (has Pillow) inside MSYS2
exec /c/Windows/py.exe "$@" 2>/dev/null || exec /c/Python*/python.exe "$@" 2>/dev/null || exec python "$@"
