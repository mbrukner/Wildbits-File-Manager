#!/bin/sh
# Compatibility entry point for the former build script.
set -eu
exec "$(dirname "$0")/build.sh" "$@"
