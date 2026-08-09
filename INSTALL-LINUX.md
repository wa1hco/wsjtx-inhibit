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

**TX Inhibit** only decides whether this **WSJT-X station** (app, PC, radio, antenna) may **assert PTT** (RTS or DTR). It is **not** **Halt Tx**.

1. **Settings → Radio** → **PTT method** = **RTS** or **DTR** (not **CAT** method, not **VOX** alone).
2. **Enable TX Inhibit** = checked (default is **off** — stock PTT until you opt in).
3. **PTT port** = a real serial device (for example `/dev/ttyUSB0`).  
   - Do **not** choose the special list value **CAT**.  
   - **Same device as CAT** is valid (shared USB CAT + RTS/DTR); a separate PTT adapter is also fine.
4. Wire RTS or DTR to the radio’s PTT/SEND, or use the radio’s USB SEND / PC KEYING map.
5. On the radio, turn **VOX off** for the test so only the key line can key the transmitter.
6. Your user may need membership in the **`dialout`** group (or **`uucp`** on some distros); log out and back in after changing groups.
7. **Shared USB checklist:** **Handshake = None**; radio menu maps the line to SEND/PTT; one app owns the modem lines. Brand notes: [docs/TX_INHIBIT.md — Shared USB CAT + RTS/DTR](docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr).

When the KEY agent has said **not to transmit**, the status bar shows a red **TX INHIBITED** badge. The WSJT-X station should **not assert PTT** even if WSJT-X is in a TX cycle.

Default UDP port is **22372** (if free). Design: [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

### Optional smoke test

With the program running and settings as above, send a hold request to UDP port **22372**.  
If you have a **git clone** of this repository and Python 3:

```bash
python3 tools/send_inhibit_hold.py --ttl-ms 3000 --station TEST
# attempt TX — WSJT-X station should not assert PTT; red badge should show
python3 tools/send_inhibit_hold.py --ttl-ms 0   # release
```

You do **not** need the script for normal use — a **KEY agent** tells WSJT-X stations not to transmit (keepalive + release). See [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

### Quick check

- You launched **this** package (AppImage or installed `wsjtx`), not a distro stock binary by mistake.
- **Help → About** shows **wsjtx-inhibit** and base version **3.0.x** (mainline + TX Inhibit). The main window title starts with **wsjtx-inhibit**.
- Receive and TX behave like normal WSJT-X when the agent has not said “don’t transmit.”
- When it has, the red **TX INHIBITED** badge appears and the WSJT-X station does not assert PTT.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| No AppImage/deb/rpm in Assets | Linux not published on that release — see above |
| AppImage will not run | `chmod +x`; try `--appimage-extract-and-run`; need 64-bit glibc-ish desktop |
| `.deb` / `.rpm` dependency errors | Use **AppImage** instead, or install missing Qt/audio packages |
| Serial PTT / device permissions | Add your user to the `dialout` (or `uucp`) group, log out/in |
| Error opening serial / “TX Inhibit: cannot open …” | Wrong device node, permissions, or another program already has the port open |
| PTT never keys at all | **PTT method** RTS/DTR and **PTT port** a real `/dev/tty…` (**not** the special value “CAT”); check USB SEND / wiring |
| TX Inhibit never stops PTT | Need **Enable TX Inhibit**, RTS/DTR on a real serial PTT port; **CAT-only PTT is not filtered**. Confirm agent UDP reaches port **22372** |
| Radio keys on port open / CAT flaky with RTS PTT | Handshake **None**; polarity; multi-app; see [shared USB CAT + RTS/DTR](docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr) |
| Radio still keys while TX INHIBITED | Radio **VOX** may still key from audio — turn VOX off |

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
