#!/bin/bash
# Build the device binary and run every ARM check under qemu-arm.
set -o pipefail
here=$(cd "$(dirname "$0")" && pwd)
cd "$here/../../projects" && make PLATFORM=RGNANO -j4 2>&1 | grep -E ' error|Error [0-9]' -A3
test -f lgpt-rgnano.elf || exit 1
status=0
for check in math_check synth_release_check mod_check chorus_check crash_check eq_check limiter_check inst_eq_check sample_play_check macro_check; do
  echo "== $check"
  bash "$here/run.sh" "$here/$check.cpp" 2>&1 | grep -v '^\[' | tail -4 || status=1
done
exit $status
