#!/bin/sh
# Compile a sketch and flash it to the Identity Disc bench board, and to nothing else.
#
# The port is never guessed or taken from the command line: it is looked up from the
# board's MAC by find-disc.py. If that board is not attached, nothing is flashed.
#
# Usage: tools/flash.sh [sketch-dir]     (default: firmware/identity-disc)
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
sketch=${1:-"$repo/firmware/identity-disc"}

cli=${ARDUINO_CLI:-"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"}
command -v arduino-cli >/dev/null 2>&1 && cli=arduino-cli

port=$(python3 "$here/find-disc.py") || { echo "flash: disc not found, nothing flashed" >&2; exit 1; }
echo "flash: disc is on $port"

"$cli" compile "$sketch"
"$cli" upload --port "$port" "$sketch"
