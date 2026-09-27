#!/bin/bash
# Render every golden sound on the device's own code under qemu-arm:
# presets and shipped samples (audio_golden_check.cpp) and the first
# seconds of every demo song (engine_room_check.cpp). Several qemu
# processes share the work. Called by tools/golden_audio.py.
#   golden_audio.sh <out dir> [jobs] [seconds per demo]
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
proj="$root/projects"
sysroot="$root/sdk/FunKey-sdk-DrUm78/arm-funkey-linux-musleabihf/sysroot"
qemu="$HOME/qemu-local/root/usr/bin/qemu-arm-static"
out="$1"
jobs="${2:-6}"
demoSeconds="${3:-10}"
if [ -z "$out" ]; then
  echo "usage: golden_audio.sh <out dir> [jobs] [seconds per demo]"
  exit 2
fi
mkdir -p "$out"
rm -f "$out"/*.wav "$out"/index-*.txt "$out"/*.log

if [ -z "$GOLDEN_NO_BUILD" ]; then
  (cd "$proj" && make PLATFORM=RGNANO -j4 2>&1 | grep -E ' error|Error [0-9]' -A3) || true
  test -f "$proj/lgpt-rgnano.elf"
  HARNESS_BUILD_ONLY=1 bash "$here/run.sh" "$here/audio_golden_check.cpp"
  HARNESS_BUILD_ONLY=1 bash "$here/run.sh" "$here/engine_room_check.cpp"
fi
bin="$proj/buildRGNANO/harness"
run() { "$qemu" -L "$sysroot" "$sysroot/lib/libc.so" "$@"; }

pids=()
for ((k = 0; k < jobs; k++)); do
  GOLDEN_OUT="$out" GOLDEN_SHARD="$k/$jobs" GOLDEN_SAMPLES="$proj/resources/samples" \
    run "$bin/audio_golden_check" > "$out/shard-$k.log" 2>&1 &
  pids+=($!)
done
# Demo songs: the first seconds from the top, one pass, no cost passes
demos=()
if [ -z "$GOLDEN_ONLY" ] || [[ "demo" == *"$GOLDEN_ONLY"* ]] || [[ "$GOLDEN_ONLY" == demo* ]]; then
  for d in "$proj"/resources/demos/lgpt_*; do
    demos+=("$(basename "$d" | sed 's/^lgpt_//')")
  done
fi
running=0
demoPids=()
for name in "${demos[@]}"; do
  id="demo-$(echo "$name" | tr 'A-Z' 'a-z')"
  if [ -n "$GOLDEN_ONLY" ] && [[ "$id" != *"$GOLDEN_ONLY"* ]]; then continue; fi
  (cd "$proj" && ENGINE_ROOM_DEMO="$name" ENGINE_ROOM_SECONDS="$demoSeconds" ENGINE_ROOM_PASSES=1 \
    ENGINE_ROOM_WAV="$out/$id.wav" run "$bin/engine_room_check" > "$out/$id.log" 2>&1 || true; \
    echo -e "$id\tdemo" > "$out/index-$id.txt") &
  demoPids+=($!)
done
status=0
for p in "${pids[@]}"; do wait "$p" || status=1; done
for p in "${demoPids[@]}"; do wait "$p" || true; done
cat "$out"/shard-*.log | grep -E "could not|FAILED|rror" || true
exit $status
