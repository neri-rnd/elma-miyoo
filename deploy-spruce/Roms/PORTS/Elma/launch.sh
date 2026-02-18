#!/bin/sh
# Elasto Mania - Miyoo Mini launch wrapper (SpruceOS)
# SpruceOS runs .sh scripts directly — no launch_standalone.sh wrapper.
# We must set SDL env vars and LD_LIBRARY_PATH ourselves.

progdir=$(dirname "$0")
cd "$progdir"

export HOME="$progdir"
export SDL_VIDEODRIVER=mmiyoo
# Don't set SDL_AUDIODRIVER — the parasyte SDL2 auto-selects its only backend.
# Explicitly setting it can fail if the driver name doesn't match at runtime.
unset SDL_AUDIODRIVER

# Bundled libs first, then SpruceOS system libs, then MI GFX hardware libs
export LD_LIBRARY_PATH="$progdir/libs:$progdir:/mnt/SDCARD/spruce/miyoomini/lib:/config/lib:/customer/lib"

# Performance mode
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null

# Kill audioserver — it holds the audio device and blocks SDL2 audio
killall audioserver 2>/dev/null
sleep 0.3

# Force FB geometry (prevents margins on first launch)
fbset -g 640 480 640 960 32

# Debug log
echo "=== Elma launch $(date) ===" > "$progdir/debug.log"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> "$progdir/debug.log"
echo "pwd=$(pwd)" >> "$progdir/debug.log"
echo "--- running ---" >> "$progdir/debug.log"

./elma >> "$progdir/debug.log" 2>&1
echo "exit code: $?" >> "$progdir/debug.log"

# Restart audioserver and restore governor
audioserver &
echo ondemand > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null
