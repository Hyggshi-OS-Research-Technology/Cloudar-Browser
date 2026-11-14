#!/usr/bin/env bash
# Run Cloudar Browser safely with necessary env variable cleanups.
# Qt WebEngine starts Chromium helper processes; Snap's GTK/runtime variables
# can leak into them and cause crashes or library conflicts.
set -euo pipefail

for variable in ${!SNAP_@}; do
    unset "$variable"
done

unset GTK_EXE_PREFIX GTK_PATH GTK_IM_MODULE_FILE
unset GIO_MODULE_DIR GIO_LAUNCHED_DESKTOP_FILE GIO_LAUNCHED_DESKTOP_FILE_PID

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build/cloudar-browser" "$@"
