#!/bin/bash
# Build a DSP check against the device's own object files (same compiler,
# same -O3/NEON flags) and run it under qemu-arm. Usage: run.sh <check.cpp>
set -e
here=$(cd "$(dirname "$0")" && pwd)
proj="$here/../../projects"
sdk="$proj/../sdk/FunKey-sdk-DrUm78"
sysroot="$sdk/arm-funkey-linux-musleabihf/sysroot"
cxx="$sdk/bin/arm-funkey-linux-musleabihf-g++"
qemu="$HOME/qemu-local/root/usr/bin/qemu-arm-static"
if [ ! -x "$qemu" ]; then
  # user-local qemu, no root needed
  mkdir -p "$HOME/qemu-local" && cd "$HOME/qemu-local"
  apt-get download qemu-user-static >/dev/null
  dpkg -x qemu-user-static_*.deb root
fi
src="$1"; name=$(basename "$src" .cpp)
mkdir -p "$proj/buildRGNANO/harness"
out="$proj/buildRGNANO/harness/$name"
flags="-DPLATFORM_RGNANO -DBUFFERED -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -O3 -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -std=gnu++03 -I$proj/../sources -I$sysroot/usr/include/SDL"
"$cxx" $flags -c "$src" -o "$out.o"
if grep -q "harness: standalone" "$src"; then
  # libc check with the app's maths replacements linked in (as the device
  # build does); static, so no dynamic loader is involved
  "$cxx" $flags -c "$proj/../sources/Adapters/DINGOO/System/RGNANOLibm.cpp" -o "$out.libm.o"
  "$cxx" -static -o "$out" "$out.o" "$out.libm.o" -lm
  "$qemu" "$out"
  exit $?
fi
objs=$(ls "$proj"/buildRGNANO/*.o | grep -v GPSDLMain.o)
"$cxx" -L"$sysroot/usr/lib" -o "$out" "$out.o" $objs -lSDL -lasound -lpthread
# The sysroot loader symlink is absolute (/lib/libc.so), so start musl's
# loader directly
"$qemu" -L "$sysroot" "$sysroot/lib/libc.so" "$out"
