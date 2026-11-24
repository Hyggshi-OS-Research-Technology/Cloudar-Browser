#!/usr/bin/env bash
# Run Cloudar Browser safely with the environment cleanups QtWebEngine needs.
#
# Two things happen here:
#
#   1. Snap/GTK variables are unset. Qt WebEngine starts Chromium helper
#      processes; Snap's GTK/runtime variables leak into them and cause crashes
#      or library conflicts.
#   2. The CPU/performance tier can be forced before launch, which is what you
#      want on a machine where the automatic detection guesses wrong (a VM with
#      a broken GPU driver, for instance). The binary honours this variable, so
#      the script only has to pass it through.
#
# Usage:
#   ./run_cpp.sh                       # classic interface (default)
#   ./run_cpp.sh --modern              # modern dark interface
#   CLOUDAR_PERF_TIER=legacy ./run_cpp.sh
#   CLOUDAR_FORCE_SOFTWARE_RENDER=1 ./run_cpp.sh
set -euo pipefail

for variable in ${!SNAP_@}; do
    unset "$variable"
done

unset GTK_EXE_PREFIX GTK_PATH GTK_IM_MODULE_FILE
unset GIO_MODULE_DIR GIO_LAUNCHED_DESKTOP_FILE GIO_LAUNCHED_DESKTOP_FILE_PID

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BINARY="${SCRIPT_DIR}/build/cloudar-browser"
if [[ ! -x "$BINARY" ]]; then
    echo "cloudar-browser has not been built yet." >&2
    echo "  cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build" >&2
    exit 1
fi

exec "$BINARY" "$@"
