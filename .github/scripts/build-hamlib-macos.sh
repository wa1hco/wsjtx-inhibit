#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: build-hamlib-macos.sh BRANCH DEPLOYMENT_TARGET PREFIX" >&2
  exit 2
fi

branch="$1"
deployment_target="$2"
prefix="$3"

git clone --depth 1 --branch "$branch" \
  https://github.com/Hamlib/Hamlib.git hamlib-src
cd hamlib-src
./bootstrap
# headerpad_max_install_names leaves room for later install_name_tool
# -add_rpath / -change when the CI bundler rewrites libusb paths into
# the app Frameworks directory. Without it, Intel rigctl-wsjtx fails
# with "larger updated load commands do not fit".
./configure \
  --prefix="$prefix" \
  --disable-shared --enable-static \
  --without-cxx-binding \
  CFLAGS="-mmacosx-version-min=${deployment_target}" \
  LDFLAGS="-mmacosx-version-min=${deployment_target} -Wl,-headerpad_max_install_names"
make -j"$(sysctl -n hw.ncpu)"
make install
