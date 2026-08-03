# Building wsjtx-wims (Linux & Windows)

This is the **application** CMake tree (see [SUPERBUILD.md](SUPERBUILD.md)).
The official project’s CI under `.github/workflows/` is preserved and is the
preferred way to produce multi-platform binaries.

## Prerequisites (both platforms)

| Dependency | Notes |
|------------|--------|
| CMake ≥ 3.7.2 | 3.16+ recommended |
| C++17 compiler | GCC/Clang (Linux), MSVC 2019+ (Windows) |
| gfortran / Intel Fortran | Decoder |
| Qt 5.12+ | Core, Gui, Widgets, Multimedia, SerialPort, Network, Sql, WebSockets, Linguist |
| FFTW3 (float + double) | `libfftw3f` |
| Boost | headers + some compiled libs as used by stock |
| Hamlib | Build from source (integration or a known-good tag) |
| libusb, readline, portaudio | As on stock WSJT-X |

## Hamlib (Linux example)

```bash
mkdir -p ~/hamlib-prefix && cd ~/hamlib-prefix
git clone https://github.com/Hamlib/Hamlib src
cd src && git checkout 4.7.0   # or integration
./bootstrap
mkdir ../build && cd ../build
../src/configure --prefix=$HOME/hamlib-prefix \
  --disable-shared --enable-static \
  --without-cxx-binding --disable-winradio \
  CFLAGS="-g -O2 -fdata-sections -ffunction-sections" \
  LDFLAGS="-Wl,--gc-sections"
make -j$(nproc) && make install-strip
```

## Linux application build

```bash
sudo apt-get install -y build-essential cmake gfortran \
  libfftw3-dev libboost-all-dev \
  qtbase5-dev qttools5-dev qtmultimedia5-dev libqt5serialport5-dev \
  libqt5sql5-sqlite libqt5websockets5-dev libqt5multimedia5-plugins \
  libusb-1.0-0-dev libudev-dev libreadline-dev portaudio19-dev \
  autoconf automake libtool pkg-config

cd /path/to/wsjtx-wims
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/hamlib-prefix"
cmake --build build -j$(nproc)
# binary: build/wsjtx  (and jt9, etc.)
```

The GitHub Action [`.github/workflows/build-linux.yml`](../.github/workflows/build-linux.yml)
installs the same packages and builds Hamlib inside the runner.

## Windows

Official CI uses [`.github/workflows/build-windows.yml`](../.github/workflows/build-windows.yml)
(MSVC, Qt, gfortran, Hamlib). Locally:

1. Install Visual Studio 2019/2022 with C++ and CMake tools.
2. Install Qt 5.x (MSVC kit) and a Fortran compiler compatible with the CI.
3. Build Hamlib for the same toolchain; set `CMAKE_PREFIX_PATH` to Qt + Hamlib.
4. Open a “x64 Native Tools” shell:

```bat
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64;C:\hamlib-prefix
cmake --build build --config Release
```

Or rely on **GitHub Actions** and download the Windows artifact from a
`build/v*` release tag.

## Release / test executables for download

```bash
# After bumping CMakeLists.txt VERSION if needed:
git tag build/v3.0.2-rc1
git push origin build/v3.0.2-rc1
```

`release.yml` builds all platforms and attaches installers/archives to a
GitHub Release. Those are the “download for testing” binaries for WIMS seats.

## Verify TX Inhibit after build

1. Run `wsjtx` with PTT method **RTS** on a USB-serial port (or UDP-only if
   you only need badge/datagram tests without RF).
2. Send a hold:

```bash
echo -n '{"tx_inhibit":1,"ttl_ms":2000,"station":"TEST","band":"144","seq":1}' \
  | nc -u -w1 127.0.0.1 22372
```

3. Status bar should show **TX INHIBITED — held by TEST** for ~2 s, then clear.
4. With a PTT dongle, confirm RTS drops while held even if Enable Tx is on.
