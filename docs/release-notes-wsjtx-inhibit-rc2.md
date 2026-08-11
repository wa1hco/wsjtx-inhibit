# wsjtx-inhibit 3.0.2-rc2

**Who this is for:** operators who already use WSJT-X and want to try **TX Inhibit**.

**What this is:** WSJT-X mainline v3.0.2 plus a low-latency **TX Inhibit** function.
Same modes and sequencing as mainline; the radio's PTT key line can be held off within
milliseconds when a multi-op interlock requires it, **without** aborting FT8/FT4
sequencing (that is what **Halt Tx** does — this is different).

Independent GPL-3 build. Not an official WSJT-X / ARRL release.

---

## What changed since rc1

rc1 shipped with several packaging problems that made it awkward to test safely
alongside a working WSJT-X install. Those are the focus of rc2.

| Area | rc1 | rc2 |
|---|---|---|
| **Install directory** | `C:\WSJT\wsjtx` — the same folder official WSJT-X uses | **`C:\WSJT\wsjtx-inhibit`** — separate |
| **Add/Remove Programs** | Same display name as official WSJT-X, indistinguishable in the list | **wsjtx-inhibit (WSJT-X with TX Inhibit)** |
| **Upgrading between RCs** | Each RC could leave its own uninstall entry | One entry, upgraded in place |
| **Package/file names** | `wsjtx-<version>-win64.exe` | `wsjtx-inhibit-<version>-win64.exe` |
| **Version shown in Help → About** | Could read `-devel` on a file named `-rc…` | Matches the file name |
| **Linux `.deb`** | Named `wsjtx`; could collide with the distro package | Named `wsjtx-inhibit`, declaring `Provides`/`Replaces`/`Conflicts` on `wsjtx` |
| **KEY test helper** | Documentation still said "press Spacebar" and named a binary that no longer exists | Grave/backtick, correct binary names |

Documentation also gained the three things testers kept hitting: the Windows Firewall
prompt, what "nothing happens" means, and the shared-settings caveat when running
beside official WSJT-X.

**No change to the TX Inhibit behaviour itself in rc2.** The protocol, timing, and the
`assert PTT ⇔ want_tx and not hold` rule are unchanged from rc1.

---

## Download (Windows 64-bit)

Under **Assets** on this release:

| File | Type | What you do |
|---|---|---|
| `wsjtx-inhibit-3.0.2-rc2-win64.exe` | **Installer** | Double-click, follow the wizard |

rc2 is installer-only on Windows (rc1's portable ZIP returns in a later build).

Linux packages are under Assets too: `.deb` (Debian/Ubuntu/Mint), `.rpm`
(Fedora/openSUSE), and `.AppImage` (any distro — `chmod +x`, then run), each for
x86_64 and aarch64 (Raspberry Pi 4/5 and other 64-bit ARM).

Windows may show **SmartScreen** on an unsigned test build: **More info** →
**Run anyway**, if you trust `wa1hco/wsjtx-inhibit`.

---

## Running beside official WSJT-X

Program files are separate as of rc2. **Settings and logs are still shared** —
both read `%LOCALAPPDATA%\WSJT-X\` (`WSJT-X.ini`, `ALL.TXT`, `wsjtx_log.adi`).

To give this build its own configuration, launch it with:

```text
"C:\WSJT\wsjtx-inhibit\bin\wsjtx.exe" --rig-name inhibit
```

You will re-enter callsign, rig, and audio once. Full steps:
[INSTALL-WINDOWS.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL-WINDOWS.md#can-i-keep-official-wsjt-x-installed).

---

## Settings for TX Inhibit

1. **File → Settings → Radio**
2. **PTT method** = **RTS** or **DTR** (CAT-only PTT is *not* filtered)
3. **Enable TX Inhibit** = checked (default off — stock PTT until you opt in)
4. **PTT port** = a real `COMx` (not the list value "CAT"); same COM as CAT is fine
5. Radio **VOX off**, **Handshake = None**

When a hold is active the status bar shows a red **TX INHIBITED** badge and the radio
stays unkeyed while WSJT-X may still be in a TX cycle in software.

Default UDP port: **22372**.

---

## Testing it

Helpers ship next to the app:

```text
bin\inhibit-test-gui.exe    Windows GUI — grave/backtick ` or the big button
bin\inhibit-test.exe        console
```

Hold **grave** (`` ` ``, left of the **1** key — **not Space**) → red badge, radio must
not key. Release → badge clears.

Or send a hold by hand:

```powershell
$u=[System.Net.Sockets.UdpClient]::new()
$b=[Text.Encoding]::UTF8.GetBytes('{"tx_inhibit":1,"ttl_ms":3000,"station":"TEST"}')
$u.Send($b,$b.Length,"127.0.0.1",22372)
```

---

## Known issues in rc2

Please read these before reporting — they are known and being worked on.

- **No indication that TX Inhibit is armed but not receiving.** If the agent is not
  running, is pointed at the wrong address, or the firewall is blocking it, this build
  transmits normally with no warning. Always confirm with a test hold.
- **If port 22372 is already in use**, an ephemeral port is used instead and the agent's
  packets will not arrive. There is currently no visible warning.
- **Badge does not appear with rig types DX Lab Suite Commander, Ham Radio Deluxe,
  OmniRig, or TCI.** Inhibit still works and PTT is still held off — only the on-screen
  badge and telemetry are missing.
- **Not tested on macOS.**
- Changing the system clock (NTP step, time-sync tools) while a hold is active can
  extend or shorten that hold.

---

## Feedback

Please include: Windows version, rig, **PTT method and exact COM port**, whether normal
FT8 worked, whether the red badge appeared, and whether the radio keyed when it should
not have.

Issues: https://github.com/wa1hco/wsjtx-inhibit/issues

Do **not** report this fork to the official WSJT-X project as a stock bug.

---

## Safety and license

- Use a **dummy load** or controlled power for first TX tests.
- This is a **release candidate** for operator testing.
- **GPL-3**, same family as WSJT-X. Separate from the official WSJT project.

| Link | Topic |
|---|---|
| [INSTALL.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL.md) | Install overview |
| [INSTALL-WINDOWS.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL-WINDOWS.md) | Windows step-by-step |
| [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md) | Design, protocol, KEY agent |
