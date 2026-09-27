#!/bin/sh

# The firmware's gba_launch_gpsp.sh, unchanged apart from save sharing.
# Launch in background, record the PID (power button save/quit), wait,
# erase it. No asound.conf: saturated sound. The in-game save is shared
# with the other GBA launcher.
. "$(dirname "$0")/sync_sav.sh"
sync_sav "$1"
rw
mv -f /etc/asound.conf /etc/asound.conf.BAK
picoarch /usr/games/gpsp_libretro.so "$1"&
pid record $!
wait $!
pid erase
mv -f /etc/asound.conf.BAK /etc/asound.conf
ro
sync_sav "$1"
