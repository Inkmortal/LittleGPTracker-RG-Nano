#!/bin/bash
# Builds a user-mode qemu-arm with TCG plugin support (the distro's
# qemu-user builds have none) and the a7cost plugin, into ~/qemu-plugin.
# No root needed: build tools and headers come from .deb files unpacked in
# the home directory. Run by run.sh the first time a check needs it.
#   ~/qemu-plugin/qemu-arm        the emulator
#   ~/qemu-plugin/liba7cost.so    the cost plugin (rebuilt when a7cost.c changes)
set -e
here=$(cd "$(dirname "$0")" && pwd)
dest="$HOME/qemu-plugin"
ver=8.2.2
src="$HOME/qemu-src/qemu-$ver"
mkdir -p "$dest"

if [ ! -x "$dest/qemu-arm" ]; then
  echo "a7cost: building qemu-arm $ver with plugins (once, a few minutes)" >&2
  mkdir -p "$HOME/qemu-src" && cd "$HOME/qemu-src"
  [ -f qemu-$ver.tar.xz ] || curl -sSLO "https://download.qemu.org/qemu-$ver.tar.xz"
  [ -d "$src" ] || tar xf qemu-$ver.tar.xz
  # glib headers, pkg-config, pip/setuptools wheels for qemu's configure
  deps="$HOME/qemu-deps"
  mkdir -p "$deps" && cd "$deps"
  apt-get download libglib2.0-dev libglib2.0-dev-bin pkgconf pkgconf-bin libpkgconf3 \
    python3-pip-whl python3-setuptools-whl libpcre2-dev libffi-dev libmount-dev \
    libselinux1-dev libblkid-dev libsepol-dev zlib1g-dev >/dev/null
  mkdir -p root && for d in *.deb; do dpkg -x "$d" root; done
  R="$deps/root"
  L="$R/usr/lib/x86_64-linux-gnu"
  # the -dev symlinks point at runtime libraries the system already has
  for f in "$L"/*.so; do
    [ -L "$f" ] || continue
    b=$(basename "$(readlink "$f")")
    if [ ! -e "$L/$b" ] && [ -e "/usr/lib/x86_64-linux-gnu/$b" ]; then
      ln -sf "/usr/lib/x86_64-linux-gnu/$b" "$f"
    fi
  done
  mkdir -p "$deps/bin" "$deps/pylib"
  cat > "$deps/bin/pkg-config" <<EOF
#!/bin/bash
export LD_LIBRARY_PATH=$L
export PKG_CONFIG_SYSROOT_DIR=$R
export PKG_CONFIG_LIBDIR=$L/pkgconfig:$R/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
exec $R/usr/bin/pkgconf "\$@"
EOF
  chmod +x "$deps/bin/pkg-config"
  cd "$deps/pylib"
  python3 - "$src/python/wheels" "$R/usr/share/python-wheels" <<'PY'
import sys, os, glob, zipfile
for d in sys.argv[1:]:
    for w in glob.glob(os.path.join(d, "*.whl")):
        zipfile.ZipFile(w).extractall(".")
PY
  if ! command -v ninja >/dev/null; then
    cd "$deps/bin"
    curl -sSL -o ninja.zip https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip
    python3 -c "import zipfile; zipfile.ZipFile('ninja.zip').extractall('.')"
    chmod +x ninja
  fi
  export PATH="$deps/bin:$R/usr/bin:$PATH"
  export PYTHONPATH="$deps/pylib"
  cd "$src" && rm -rf build && mkdir build && cd build
  ../configure --target-list=arm-linux-user --enable-plugins --disable-system --disable-tools \
    --disable-docs --disable-werror --disable-capstone --disable-slirp >/dev/null
  nice ninja -j3 qemu-arm >/dev/null
  cp qemu-arm "$dest/qemu-arm"
fi

if [ ! -f "$dest/liba7cost.so" ] || [ "$here/a7cost.c" -nt "$dest/liba7cost.so" ]; then
  gcc -shared -fPIC -O2 -Wall -I"$src/include/qemu" "$here/a7cost.c" -o "$dest/liba7cost.so"
fi
