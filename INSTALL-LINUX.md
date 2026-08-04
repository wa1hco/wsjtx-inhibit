# Install on Linux — wsjtx-inhibit

**Audience: operators and testers.**  
How to download and run the Linux **wsjtx-inhibit** packages (WSJT-X + TX Inhibit).

You do **not** need to compile from source or use CMake for a normal test.

---

## Where do I get the program?

Linux packages are **not** stored in the git folders you browse on GitHub.  
They only appear as **downloadable files** on a Release.

1. Open **[https://github.com/wa1hco/wsjtx-inhibit/releases](https://github.com/wa1hco/wsjtx-inhibit/releases)**.
2. Click the release you were asked to use.
3. Scroll to **Assets** at the bottom.

### What should I look for in Assets?

| File name ends with | For whom | Notes |
|---------------------|----------|--------|
| **`.AppImage`** | Most people on 64-bit Linux | **Preferred.** No root required. |
| **`.deb`** | Debian, Ubuntu, Mint, Pop!_OS, … | Installs like other `.deb` packages |
| **`.rpm`** | Fedora, Rocky, RHEL, openSUSE, … | Installs like other `.rpm` packages |

Example names (version numbers change):

```text
wsjtx-3.0.2-linux-x86_64.AppImage
wsjtx-3.0.2-linux-x86_64.deb
wsjtx-3.0.2-linux-x86_64.rpm
```

On ARM computers, names may say `aarch64` instead of `x86_64`.

### I only see Windows `.exe` / `.zip` under Assets

Then **this release has no Linux build yet.**

- Do not search the repository tree for `.deb` / AppImage files — they will not be there.
- Use a different release that lists Linux assets, or ask the maintainer to publish a Linux package.
- Overview of all platforms: [INSTALL.md](INSTALL.md).

---

## Choose one package type

| Situation | Download |
|-----------|----------|
| Unsure which Linux you have, or mixed group of testers | **AppImage** |
| Everyone on Ubuntu / Debian / Mint | **`.deb`** is fine (AppImage still easiest) |
| Everyone on Fedora / Rocky / openSUSE | **`.rpm`** is fine (AppImage still easiest) |

Install **one** method only; you do not need all three files.

---

## Method A — AppImage (recommended)

1. From **Assets**, download the file ending in **`.AppImage`** for your CPU (`x86_64` or `aarch64`).
2. Open a terminal in the download folder.
3. Make it executable and run it:

```bash
chmod +x wsjtx-*-linux-*.AppImage
./wsjtx-*-linux-*.AppImage
```

If your system complains about FUSE / “not mountable”:

```bash
./wsjtx-*-linux-*.AppImage --appimage-extract-and-run
```

No `sudo` required. You can move the AppImage anywhere (Desktop, `~/bin`, etc.).

---

## Method B — Debian / Ubuntu (`.deb`)

Only if **Assets** includes a `.deb` file.

1. Download the `.deb` (same architecture as your PC).
2. In the download folder:

```bash
sudo apt install ./wsjtx-*-linux-*.deb
```

If that fails, try:

```bash
sudo dpkg -i ./wsjtx-*-linux-*.deb
sudo apt-get install -f
```

3. Start from the application menu, or run:

```bash
wsjtx
```

**Conflict with distro WSJT-X:** the package may be named `wsjtx`. If install complains, remove the distro package first (only if you are OK replacing it for this test):

```bash
sudo apt remove wsjtx
```

Then install the `.deb` again. Prefer AppImage if you want to keep distro WSJT-X untouched.

---

## Method C — Fedora / RHEL / openSUSE (`.rpm`)

Only if **Assets** includes an `.rpm` file.

```bash
sudo dnf install ./wsjtx-*-linux-*.rpm
# older systems:
# sudo rpm -Uvh ./wsjtx-*-linux-*.rpm
```

Then run `wsjtx` from the menu or terminal.  
Same note as `.deb`: may conflict with a distro package named `wsjtx`.

---

## First-time settings

Same as stock WSJT-X:

1. **File → Settings → General** — callsign, grid.
2. **Settings → Radio** — rig, CAT device, baud rate.
3. **Settings → Audio** — input/output devices.

### For TX Inhibit testing

1. **Settings → Radio** → **PTT method** = **RTS** or **DTR**.
2. Choose the serial device used for PTT (often not the same as CAT).
3. Wire RTS/DTR to radio PTT as for any digital interface.

See [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md).

### Quick check

- **Help → About** shows **3.0.x** for this build.
- Receive and TX behave like normal WSJT-X when inhibit is not active.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| No AppImage/deb/rpm in Assets | Linux not published on that release — see above |
| AppImage will not run | `chmod +x`; try `--appimage-extract-and-run`; need 64-bit glibc-ish desktop |
| `.deb` / `.rpm` dependency errors | Use **AppImage** instead, or install missing Qt/audio packages |
| Serial PTT / device permissions | Add your user to the `dialout` (or `uucp`) group, log out/in |
| Inhibit never engages | Need RTS/DTR PTT path; CAT-PTT alone is not gated in this build |

---

## Feedback

[https://github.com/wa1hco/wsjtx-inhibit/issues](https://github.com/wa1hco/wsjtx-inhibit/issues)

---

## Maintainers only (not for operators)

How Linux packages are built and named is described in CI (`.github/workflows/build-linux.yml`) and [docs/BUILDING.md](docs/BUILDING.md). Operators only need files attached under Release **Assets**.

Local packaging after a source build (developers):

```bash
# from cmake build directory — examples
cpack -G DEB
cpack -G RPM
# AppImage: see CI “Package AppImage” / scripts/linux/
```

Not an official WSJT-X release. GPL-3.
