#!/usr/bin/env bash
set -euo pipefail

# Directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Base flags required by the toolchain
BASE_FLAGS=(
    -ffreestanding
    -static
    -I"$SCRIPT_DIR"
    -I"$SCRIPT_DIR/libc"
)

# Forward all user‑supplied arguments to gcc
gcc "${BASE_FLAGS[@]}" "$@"
