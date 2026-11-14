#!/bin/bash
# Launch script for Cloudar Browser with library path fix

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Fix for libpthread symbol lookup error
export LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0

# Run the browser
python3 main.py
