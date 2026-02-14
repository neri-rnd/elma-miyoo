#!/bin/sh
# Elasto Mania - Miyoo Mini launch wrapper
# Called by OnionOS launch_standalone.sh (which handles audioserver, perf mode)

progdir=$(dirname "$0")
cd "$progdir"

export HOME="$progdir"

# Bundled SDL2 libs first, then system paths for MI GFX hardware libs
# Pattern from steward-fu/sdl2 README: LD_LIBRARY_PATH=.:/config/lib:/customer/lib
export LD_LIBRARY_PATH="$progdir/libs:$progdir:/config/lib:/customer/lib"

# No need to set SDL_VIDEODRIVER/SDL_AUDIODRIVER — the "Mini" driver
# is the only one compiled into this SDL2, so it's auto-selected.

# Debug: log environment
echo "=== Elma launch $(date) ===" > "$progdir/debug.log"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> "$progdir/debug.log"
echo "pwd=$(pwd)" >> "$progdir/debug.log"
echo "--- running ---" >> "$progdir/debug.log"

# Force FB mode to 640x480 with double-buffer virtual height;
# prevents margins on first launch after boot
fbset -g 640 480 640 960 32

./elma >> "$progdir/debug.log" 2>&1
echo "exit code: $?" >> "$progdir/debug.log"
