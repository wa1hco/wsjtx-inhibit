#!/usr/bin/env bash
# Package .deb and .rpm from an existing CMake build directory (Linux).
# Usage: scripts/linux/package-cpack.sh <build-dir> <version> [arch]
set -euo pipefail

BUILD_DIR="${1:?build dir}"
VERSION="${2:?version e.g. 3.0.2}"
ARCH="${3:-}"

if [ -z "$ARCH" ]; then
  case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    aarch64|arm64) ARCH=aarch64 ;;
    *) ARCH="$(uname -m)" ;;
  esac
fi

cd "$BUILD_DIR"

echo "=== cpack DEB version=$VERSION arch=$ARCH ==="
cpack -G DEB -D "CPACK_DEBIAN_FILE_NAME=wsjtx-${VERSION}-linux-${ARCH}.deb"
ls -lh "wsjtx-${VERSION}-linux-${ARCH}.deb"
file "wsjtx-${VERSION}-linux-${ARCH}.deb"
dpkg-deb --info "wsjtx-${VERSION}-linux-${ARCH}.deb" | head -20 || true

echo "=== cpack RPM ==="
cpack -G RPM -D "CPACK_PACKAGE_FILE_NAME=wsjtx-${VERSION}-linux-${ARCH}"
# CPack may emit .rpm with the file name above
ls -lh ./*.rpm
RPM="$(ls -1 wsjtx-${VERSION}-linux-${ARCH}*.rpm 2>/dev/null | head -1 || ls -1 ./*.rpm | head -1)"
echo "RPM=$RPM"
rpm -qpi "$RPM" 2>/dev/null | head -20 || true

echo "Done. Artifacts in: $BUILD_DIR"
