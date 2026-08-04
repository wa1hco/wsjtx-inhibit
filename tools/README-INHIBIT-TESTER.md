# TX Inhibit spacebar tester

## Windows (operators — preferred)

**`inhibit_spacebar.exe`** ships **in the install** next to `wsjtx.exe`:

```text
C:\WSJT\wsjtx\bin\inhibit_spacebar.exe
```

Also on the Start menu as **TX Inhibit Spacebar Tester** (NSIS install).

1. Start **wsjtx-inhibit** with **PTT method = RTS or DTR**.
2. Run **`inhibit_spacebar.exe`** (from install `bin\`).
3. **Click** the big button → inhibit; **click again** → clear (toggle).  
   Or **hold SPACE** while pressed; release space to clear.
4. If stuck: **Force RELEASE now** or Escape.

No Python, no PowerShell, no extra runtime — only Windows system libraries.

| Action | Packet |
|--------|--------|
| Button **click** | Toggle hold on/off |
| Space **down** | Hold + keepalives (`ttl_ms` default 600) |
| Space **up** / Force RELEASE / Escape | Release (`ttl_ms: 0`) |

If the WSJT-X badge says **local KEY line**, CTS on that COM port is asserted
(often a floating pin). The gate ignores CTS until it has seen CTS idle (low)
after open.

## Build (developers)

Windows/MinGW or MSVC, via main CMake tree (`WIN32` only):

```text
tools/inhibit_spacebar/   → target inhibit_spacebar → bin/inhibit_spacebar.exe
```

Standalone MinGW:

```bash
gcc -O2 -mwindows -o inhibit_spacebar.exe inhibit_spacebar.c \
  -lws2_32 -lcomctl32 -luser32 -lgdi32
```

## Optional: Python (maintainers)

`inhibit_spacebar_gui.py` and `send_inhibit_hold.py` remain for Linux dev or scripting.
They are **not** required for Windows operator testing.
