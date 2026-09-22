#!/bin/sh
# Audio + system report from the RG Nano itself, run over SSH:
#
#   tools/rgnano-ssh.sh "sh -s" < tools/rgnano-device-diag.sh
#
# Answers the questions the simulator cannot: is the CPU keeping up, is ALSA
# dropping buffers (xruns), what is the app actually doing?

echo "=== uname / uptime ==="
uname -a
uptime
echo
echo "=== cpu ==="
grep -E "model name|BogoMIPS|Hardware" /proc/cpuinfo | head -5
for f in /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq \
         /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor \
         /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq; do
  [ -r "$f" ] && echo "$(basename $f): $(cat $f)"
done
echo
echo "=== top processes ==="
top -b -n 1 2>/dev/null | head -12 || ps
echo
echo "=== the tracker ==="
pid=$(pidof lgpt-rgnano.elf 2>/dev/null || pgrep -f lgpt-rgnano | head -1)
if [ -n "$pid" ]; then
  echo "pid $pid"
  cat /proc/$pid/status 2>/dev/null | grep -E "Name|State|Threads|VmRSS"
  echo "--- threads (utime/stime in ticks) ---"
  for t in /proc/$pid/task/*; do
    [ -r "$t/stat" ] || continue
    awk '{printf "tid %s %-16s utime=%s stime=%s prio=%s\n", $1, $2, $14, $15, $18}' "$t/stat"
  done
else
  echo "not running (start a song on the device first)"
fi
echo
echo "=== ALSA playback status (xruns = dropouts) ==="
for s in /proc/asound/card*/pcm*p/sub*/status; do
  [ -r "$s" ] || continue
  echo "--- $s"
  cat "$s"
done
cat /proc/asound/card0/pcm0p/sub0/hw_params 2>/dev/null
echo
echo "=== mixer ==="
amixer 2>/dev/null | head -20
echo
echo "=== battery / power ==="
for f in /sys/class/power_supply/*/capacity /sys/class/power_supply/*/status \
         /sys/class/power_supply/*/voltage_now /sys/class/power_supply/*/current_now; do
  [ -r "$f" ] && echo "$f: $(cat $f)"
done
echo
echo "=== the app's own load log ==="
tail -20 /mnt/Applications/Tracks/lgpt-perf.log 2>/dev/null || echo "no lgpt-perf.log yet"
echo
echo "=== kernel messages (audio / usb) ==="
dmesg 2>/dev/null | grep -iE "underrun|xrun|audio|sound|codec|usb" | tail -15
