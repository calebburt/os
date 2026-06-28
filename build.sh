#!/usr/bin/env bash
set -euo pipefail

# Directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Base flags required by the toolchain.
# -I "$SCRIPT_DIR/libc" comes first so our headers shadow the system
# <stdio.h>, <stdlib.h>, <string.h> when programs use angle-bracket includes.
BASE_FLAGS=(
    -ffreestanding
    -nostdlib
    -static
    -I "$SCRIPT_DIR/libc"
    -I "$SCRIPT_DIR"
)

# Always link the full libc implementation.
# crt0.c provides _start → main() bridging; omit it for programs
# that define their own _start.
LIBC_SRCS=(
    "$SCRIPT_DIR/libc/crt0.c"
    "$SCRIPT_DIR/libc/stdio.c"
    "$SCRIPT_DIR/libc/stdlib.c"
    "$SCRIPT_DIR/libc/string.c"
    "$SCRIPT_DIR/libc/math.c"
)

# Forward all user‑supplied arguments to gcc, then append libc sources
gcc "${BASE_FLAGS[@]}" "$@" "${LIBC_SRCS[@]}"
