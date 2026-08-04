# WSJT-X superbuild vs application tree

This repository is the **application source tree** for WSJT-X (the program
you run). Official builds and this fork both use CMake against this tree.
There is a second packaging form you will see online called a **superbuild**.

## Application tree (this repo)

```
wsjtx-inhibit/       ← you are here
  CMakeLists.txt     ← builds wsjtx, jt9, wsprd, …
  Configuration.cpp
  widgets/
  Transceiver/
  TxInhibit/         ← TX Inhibit
  Network/
  lib/               ← Fortran decoder
  …
```

**Dependencies are external.** You install or build Hamlib, Qt 5, FFTW3,
Boost, gfortran, etc., then:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/hamlib-prefix;…"
cmake --build build -j$(nproc)
```

This is what the GitHub Actions workflows (`.github/workflows/build-linux.yml`,
`build-windows.yml`, `build-macos.yml`) do on every platform.

## Superbuild tree (separate packaging form)

A **superbuild** is a thin CMake project that does **not** contain the full
application sources. It:

1. Downloads or unpacks **Hamlib** and **WSJT-X** source tarballs (or clones
   git),
2. Builds Hamlib into a prefix,
3. Builds WSJT-X against that prefix,
4. Optionally produces a single “source package” for distro maintainers.

Typical layout of a superbuild drop (e.g. Improved releases):

```
wsjtx-3.1.0/                 ← superbuild root
  CMakeLists.txt             ← orchestrates ExternalProject builds
  src/
    hamlib-4.7.1.tar.gz
    wsjtx.tgz                ← the *application* tree as a tarball
  build/                     ← after configure: extracted sources + objects
    wsjtx-prefix/src/wsjtx/  ← extracted application tree (what this repo is)
```

So:

| Concept | Superbuild | This repo (`wsjtx-inhibit`) |
|---------|------------|---------------------------|
| Role | Build *orchestrator* + source bundles | The actual WSJT-X program sources |
| Contains `widgets/mainwindow.cpp` | Only after extract, under `build/…` | Yes, at the root |
| Hamlib | Built as an ExternalProject | You supply via `CMAKE_PREFIX_PATH` |
| Good for | Distro packages, “one tarball builds all” | Day-to-day development, CI, patches |

## Why this project uses the application tree

The TX Inhibit work is a small set of C++ files and hooks inside the program.
Diffing, reviewing, and rebasing those changes is far simpler against the
application tree than against a nested `build/wsjtx-prefix/src/wsjtx` extract
from a superbuild. When a new official tag ships, we re-import that application
tree (or merge from `WSJTX/wsjtx`) and re-apply the `TxInhibit` commit.

## Historical Improved superbuild drop

The local Improved drop used for analysis was:

`~/ham/wsjtx-3.1.0_improved_AL_PLUS_260522/wsjtx-3.1.0/`

Its extracted application sources were also published as
[wa1hco/wsjtx-improved-wims](https://github.com/wa1hco/wsjtx-improved-wims).
**This repo (`wsjtx-inhibit`) is based on official mainline**, not Improved, so the
inhibit patch can be offered cleanly to either lineage.
