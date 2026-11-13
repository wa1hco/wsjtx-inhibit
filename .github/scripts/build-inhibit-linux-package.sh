#!/usr/bin/env bash
# Build, test, and package the Ubuntu 24.04 inhibit .deb.
#
# The package links Qt, FFTW, and the other runtime libraries from Ubuntu so
# `apt install` is enough. Boost 1.92 (with the queue patch) and Hamlib 4.7.2
# are static, so the installed program does not need LD_LIBRARY_PATH.
set -euo pipefail

usage() {
  echo "Usage: build-inhibit-linux-package.sh [--source DIR] [--boost-prefix DIR] [--hamlib-prefix DIR]" >&2
  exit 2
}

source_dir=""
boost_prefix=""
hamlib_prefix=""
hamlib_ref="4.7.2"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --source) source_dir="$2"; shift 2 ;;
    --boost-prefix) boost_prefix="$2"; shift 2 ;;
    --hamlib-prefix) hamlib_prefix="$2"; shift 2 ;;
    --hamlib-ref) hamlib_ref="$2"; shift 2 ;;
    *) usage ;;
  esac
done

if [ -z "$source_dir" ]; then
  source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fi
source_dir="$(cd "$source_dir" && pwd)"
boost_prefix="${boost_prefix:-$source_dir/.deps/boost}"
hamlib_prefix="${hamlib_prefix:-$source_dir/.deps/hamlib}"
build_dir="${INHIBIT_BUILD_DIR:-$source_dir/wsjtx-build}"
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}"

for tool in cmake g++ gfortran python3 cpack dpkg-buildpackage pkg-config; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing build tool: $tool" >&2
    exit 1
  fi
done

# shellcheck source=.github/scripts/boost-version.sh
. "$source_dir/.github/scripts/boost-version.sh"
# shellcheck source=.github/scripts/linux-boost-validation.sh
. "$source_dir/.github/scripts/linux-boost-validation.sh"

if ! validate_linux_boost_archives "$boost_prefix" x86_64 "$BOOST_VERSION"; then
  echo "Building Boost ${BOOST_VERSION} into ${boost_prefix}"
  rm -rf "$boost_prefix"
  "$source_dir/.github/scripts/build-boost-linux.sh" "$BOOST_VERSION" "$boost_prefix"
  validate_linux_boost_archives "$boost_prefix" x86_64 "$BOOST_VERSION"
fi

if [ ! -f "$hamlib_prefix/lib/pkgconfig/hamlib.pc" ]; then
  echo "Building Hamlib ${hamlib_ref} into ${hamlib_prefix}"
  rm -rf "$hamlib_prefix"
  hamlib_work="$(mktemp -d)"
  (
    cd "$hamlib_work"
    "$source_dir/.github/scripts/build-hamlib-linux.sh" "$hamlib_ref" "$hamlib_prefix"
  )
  rm -rf "$hamlib_work"
fi
if [ ! -f "$hamlib_prefix/lib/pkgconfig/hamlib.pc" ]; then
  echo "Hamlib pkg-config file was not installed" >&2
  exit 1
fi

label="$(python3 "$source_dir/.github/scripts/inhibit-release-identity.py" --root "$source_dir" \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["label"])')"
echo "Packaging WSJT-X ${label}"

export PKG_CONFIG_PATH="${hamlib_prefix}/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
if command -v ccache >/dev/null 2>&1; then
  launch_c="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_Fortran_COMPILER_LAUNCHER=ccache"
  ccache --zero-stats || true
else
  launch_c=""
fi

# shellcheck disable=SC2086
cmake -S "$source_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBoost_USE_STATIC_LIBS=ON \
  -DBOOST_ROOT="${boost_prefix}" \
  -DBOOST_INCLUDEDIR="${boost_prefix}/include" \
  -DBOOST_LIBRARYDIR="${boost_prefix}/lib" \
  -DBoost_DIR="${boost_prefix}/lib/cmake/Boost-${BOOST_VERSION}" \
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
  -DBoost_NO_BOOST_CMAKE=OFF \
  -DCMAKE_PREFIX_PATH="${hamlib_prefix}" \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_ENABLE_TESTS=ON \
  -DWSJT_FORTRAN_LIBRARY_VARIANTS=OPENMP_ONLY \
  -DCMAKE_SKIP_BUILD_RPATH=ON \
  -DCMAKE_SKIP_INSTALL_RPATH=ON \
  $launch_c \
  -Wno-dev

cmake --build "$build_dir" -j "$jobs"

version_output="$("$build_dir/wsjtx_app_version" -v | tr -d '\r')"
expected_version="WSJT-X ${label}"
if [ "$version_output" != "$expected_version" ]; then
  echo "wsjtx_app_version -v printed '${version_output}', expected '${expected_version}'" >&2
  exit 1
fi
echo "wsjtx_app_version -v: ${version_output}"

required_tests=(
  test_inhibit_identity
  test_tx_inhibit_transceiver
  test_tx_inhibit_thread_priority
  test_message_client_inhibit_status
  test_keying_monitor
  test_pin_combiner
)
for name in "${required_tests[@]}"; do
  if ! ctest --test-dir "$build_dir" -N -R "^${name}$" | grep -q "Total Tests: 1"; then
    echo "Required test was not registered: ${name}" >&2
    ctest --test-dir "$build_dir" -N -R inhibit >&2 || true
    exit 1
  fi
  xvfb-run -a ctest --test-dir "$build_dir" --output-on-failure --timeout 180 -R "^${name}$"
done

(
  cd "$build_dir"
  cpack -G DEB -D "CPACK_DEBIAN_FILE_NAME=wsjtx-${label}-linux-x86_64.deb"
)
deb="${build_dir}/wsjtx-${label}-linux-x86_64.deb"
if [ ! -f "$deb" ]; then
  echo "cpack did not write ${deb}" >&2
  exit 1
fi

# shellcheck source=.github/scripts/linux-artifact-validation.sh
. "$source_dir/.github/scripts/linux-artifact-validation.sh"
validate_deb_package "$deb"

payload="$(mktemp -d)"
dpkg-deb -x "$deb" "$payload"
for binary in wsjtx wsjtx_app_version jt9; do
  if ldd "$payload/usr/bin/$binary" | grep -q 'not found'; then
    echo "${binary} is missing a shared library:" >&2
    ldd "$payload/usr/bin/$binary" >&2
    exit 1
  fi
  if readelf -d "$payload/usr/bin/$binary" | grep -E -q 'RPATH|RUNPATH'; then
    echo "${binary} has an RPATH; it would not run from the system library paths" >&2
    readelf -d "$payload/usr/bin/$binary" >&2
    exit 1
  fi
done
extracted_version="$(env -u LD_LIBRARY_PATH -u LD_PRELOAD -u QT_PLUGIN_PATH \
  -u QT_QPA_PLATFORM_PLUGIN_PATH "$payload/usr/bin/wsjtx_app_version" -v | tr -d '\r')"
if [ "$extracted_version" != "$expected_version" ]; then
  echo "Packaged wsjtx_app_version -v printed '${extracted_version}'" >&2
  exit 1
fi
rm -rf "$payload"

if [ "${INHIBIT_INSTALL_SMOKE:-0}" = "1" ]; then
  sudo apt-get install -y "$deb"
  installed_version="$(env -u LD_LIBRARY_PATH -u LD_PRELOAD -u QT_PLUGIN_PATH \
    -u QT_QPA_PLATFORM_PLUGIN_PATH wsjtx_app_version -v | tr -d '\r')"
  if [ "$installed_version" != "$expected_version" ]; then
    echo "Installed wsjtx_app_version -v printed '${installed_version}'" >&2
    exit 1
  fi
  gui_version="$(xvfb-run -a env -u LD_LIBRARY_PATH -u LD_PRELOAD -u QT_PLUGIN_PATH \
    -u QT_QPA_PLATFORM_PLUGIN_PATH wsjtx --version | tr -d '\r')"
  if [ "$gui_version" != "$expected_version" ]; then
    echo "wsjtx --version printed '${gui_version}'" >&2
    exit 1
  fi
  echo "Installed package runs without library path overrides: ${installed_version}"
fi

echo "Linux package: ${deb}"
