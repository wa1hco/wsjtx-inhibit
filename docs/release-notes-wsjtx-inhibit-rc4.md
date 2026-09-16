# wsjtx-inhibit 3.0.2-rc4

**Who this is for:** operators who already use WSJT-X and want to try **TX Inhibit**.

**What this is:** WSJT-X mainline v3.0.2 plus a low-latency **TX Inhibit** function.
Same modes and sequencing as mainline; the radio's PTT key line can be held off within
milliseconds when a multi-op interlock requires it, **without** aborting FT8/FT4
sequencing (that is what **Halt Tx** does — this is different).

Independent GPL-3 build. Not an official WSJT-X / ARRL release.

---

## What changed since rc3

rc3 is still the type-18 / ephemeral-port protocol. **rc4 fixes a field bug
from the W2SZ September VHF contest.**

At W2SZ, WIMS built an empty KEY-agent target list. Type 17 `InhibitStatus`
carried **port 0**. WIMS rejects port 0, so no holds reached the digi seats.

Two causes:

1. `QUdpSocket::bind(0)` can return true while `localPort()` is still 0.
   The gate then announced port 0.
2. The gate binds **after** Hamlib `rig_open`. While that is in progress,
   Enable is on and the port is still 0, so the 15 s type-17 pulse published
   port 0.

**rc4:**

- After bind, if `localPort()` is 0, read the port with `getsockname`.
- If the port is still 0, treat it as bind failure. Do not emit `portBound(0)`.
- Do not send a live type 17 with port 0. Port 0 is only the disable/clear
  announce.

Wire format is unchanged. rc3 KEY agents still work. Point them at the
ephemeral `host:port` from InhibitStatus / the tooltip (never port 0).

Authority doc: [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md).

---

## Download

Under **Assets** on this release:

| File | Platform | What you do |
|---|---|---|
| `wsjtx-inhibit-3.0.2-rc4-win64.exe` | Windows 64-bit | Double-click, follow the wizard |
| `wsjtx-3.0.2-rc4-linux-x86_64.AppImage` | Linux x86_64 | `chmod +x`, then run (preferred) |
| `wsjtx-3.0.2-rc4-linux-x86_64.deb` | Debian / Ubuntu / Mint | `sudo apt install ./…` |
| `wsjtx-3.0.2-rc4-linux-x86_64.rpm` | Fedora / RHEL / openSUSE | `sudo dnf install ./…` |
| `wsjtx-3.0.2-rc4-linux-aarch64.*` | Linux ARM64 (Pi 4/5, …) | Same pattern as x86_64 |

Windows may show **SmartScreen** on an unsigned test build: **More info** →
**Run anyway**, if you trust `wa1hco/wsjtx-inhibit`.

Install overview: [INSTALL.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL.md).

---

## Running beside official WSJT-X

Program files are separate (`C:\WSJT\wsjtx-inhibit\` on Windows).
**Settings and logs are still shared** — both read `%LOCALAPPDATA%\WSJT-X\`
(`WSJT-X.ini`, `ALL.TXT`, `wsjtx_log.adi`).

To give this build its own configuration, launch it with:

```text
"C:\WSJT\wsjtx-inhibit\bin\wsjtx.exe" --rig-name inhibit
```

Full steps:
[INSTALL-WINDOWS.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL-WINDOWS.md#can-i-keep-official-wsjt-x-installed).

---

## Settings for TX Inhibit

1. **File → Settings → Radio**
2. **PTT method** = **RTS** or **DTR** (CAT-only PTT is *not* filtered)
3. **PTT port** = a real `COMx` / `/dev/ttyUSBx` (not the list value **CAT**); same COM as CAT is fine when Handshake = None
4. **Enable TX Inhibit** = checked (only after method + port are set; default off)
5. Radio **VOX off**, **Handshake = None**

When a hold is active the status bar shows a red **INHIBIT** badge. The radio
stays unkeyed while WSJT-X may still be in a TX cycle in software.

Confirm the tooltip `host:port` is **not 0** after the rig is open. That is the
port WIMS and `inhibit-agent` must use.

**WARNING — turn radio VOX off.** TX Inhibit only gates the RTS/DTR PTT line.
If VOX is on, audio can still key the radio while the badge says **INHIBIT**.

---

## Testing it

Helpers ship next to the app:

```text
bin\inhibit-agent-gui.exe   KEY agent GUI (CTS → type 18)
bin\inhibit-agent.exe       KEY agent CLI
bin\inhibit-test.exe        console KEY stand-in (grave/backtick)
```

1. Enable TX Inhibit as above. Note the ephemeral **host:port** in the status-bar tooltip (also announced as InhibitStatus type 17). The port must not be 0.
2. Point `inhibit-agent-gui` at that `host:port` and Apply (**NEED GATE** until then).
3. Or use `inhibit-test`: hold **grave** (`` ` ``, left of **1** — **not Space**) → red **INHIBIT**, radio must not key. Release → badge clears.

Bench without a KEY dongle: Rig **None**, PTT **RTS**, PTT port **`inhibit-sim`**, Enable TX Inhibit, then use `inhibit-test`.

KEY agent design: [docs/INHIBIT_AGENT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/INHIBIT_AGENT.md).

---

## Known issues in rc4

Please read these before reporting — they are known.

- **No strong “armed but silent” alarm.** If the agent is not running, is pointed at the wrong `host:port`, or the firewall blocks UDP, this build transmits normally. Always confirm with a test hold.
- **Badge does not appear with rig types DX Lab Suite Commander, Ham Radio Deluxe, OmniRig, or TCI.** Inhibit still works and PTT is still held off — only the on-screen badge and some telemetry are missing.
- **macOS packages are not published.** CI compiles macOS; there is no operator `.pkg`.
- Changing the system clock (NTP step, time-sync tools) while a hold is active can extend or shorten that lease.

---

## Feedback

Please include: OS version, rig, **PTT method and exact COM/tty port**, whether normal
FT8 worked, whether the red **INHIBIT** badge appeared, the gate `host:port` you used,
and whether the radio keyed when it should not have.

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
| [INSTALL-LINUX.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL-LINUX.md) | Linux step-by-step |
| [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md) | Design, protocol, KEY agent |
| [docs/INHIBIT_AGENT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/INHIBIT_AGENT.md) | `inhibit-agent` |
