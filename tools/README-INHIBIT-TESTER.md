# TX Inhibit spacebar tester

## Windows (operators — preferred)

**`inhibit_spacebar.exe`** ships **in the install** next to `wsjtx.exe`:

```text
C:\WSJT\wsjtx\bin\inhibit_spacebar.exe
```

Also on the Start menu as **TX Inhibit Spacebar Tester** (NSIS install).

1. Start **wsjtx-inhibit** with **PTT method = RTS or DTR**.
2. Run **`inhibit_spacebar.exe`** (from install `bin\`).
3. **Hold SPACE** (or hold the big button) = KEY down → UDP hold + keepalives.
4. **Release** SPACE/button = KEY up → **WIMS adaptive hang** (still keepalives),
   then UDP release (`ttl_ms: 0`).
5. If stuck: **Force RELEASE now** (skips hang) or Escape.

No Python, no PowerShell, no extra runtime — only Windows system libraries.

### Adaptive hang (same rules as WIMS agent + WSJT CTS KEY)

| KEY-down duration | Hang after KEY-up |
|-------------------|-------------------|
| Dit-like ≤ 200 ms | 8 × dit, clamped 200–1000 ms |
| Long KEY ≥ 750 ms | **20 ms** |
| Mid / non-dit | **20 ms** |

Status shows `HANG` and hang milliseconds remaining.

| Action | Behavior |
|--------|----------|
| Space / mouse **down** | KEY-down → hold + keepalives |
| Space / mouse **up** | Adaptive hang, then release |
| Force RELEASE / Escape | Immediate release (no hang) |

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
