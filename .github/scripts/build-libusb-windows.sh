#!/usr/bin/env bash
# MSYS2 no longer publishes mingw-w64-x86_64-libusb (MINGW64 is deprecated;
# the package's mingw_arch is ucrt64/clang64/clangarm64 only). The Windows
# workflow stays on MINGW64 for the Windows 7 API floor, so build the last
# packaged release here and install it into the MinGW prefix.
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: build-libusb-windows.sh VERSION PREFIX" >&2
  exit 2
fi

version="$1"
prefix="$2"

case "$version" in
  1.0.30)
    expected_sha256=fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf
    ;;
  *)
    echo "No pinned SHA256 for libusb ${version}" >&2
    exit 2
    ;;
esac

curl -L --fail --retry 5 --retry-delay 10 -o libusb.tar.bz2 \
  "https://github.com/libusb/libusb/releases/download/v${version}/libusb-${version}.tar.bz2"
echo "${expected_sha256}  libusb.tar.bz2" | sha256sum -c -
tar -xjf libusb.tar.bz2
cd "libusb-${version}"

host="$(gcc -dumpmachine)"
./configure \
  --prefix="$prefix" \
  --build="$host" \
  --host="$host" \
  --enable-shared \
  --enable-static
make -j"$(nproc)"
make install
pkg-config --exists libusb-1.0
pkg-config --modversion libusb-1.0
