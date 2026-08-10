# wsjtx-inhibit — pre-rc2 review findings

**Reviewed at:** commit `eeae769` (`main`), 2026-08-09
**Baseline compared against:** `26e8bf5` (WSJT-X mainline v3.0.2, `ccdfaf3`)
**Scope:** all 54 changed files, 14 markdown docs, CI workflows, packaging scripts.
**Verification performed:** built and ran `test_tx_inhibit_logic` (12/12 pass) and
`test_http_user_agent` (fails, see R2). Markdown links/anchors checked programmatically.

**Review lenses:**
1. A WSJT-X maintainer looking at this as a fork / potential upstream contribution.
2. An operator or beta tester, most likely on Windows.

Status key: ☐ open · ☑ done · ⊘ won't fix (record why)

---

## Goal (restated 2026-08-09)

> **A maintained parallel fork of WSJT-X that rebases onto mainline as needed, with
> the eventual goal of merging the fork back into mainline.**

Not a throwaway branch, and not a one-shot PR. That has three consequences that shape
every priority below:

1. **The fork is long-lived**, so rebase cost is a recurring tax, not a one-time
   expense. Minimising the footprint in upstream-owned files is the highest-leverage
   ongoing discipline. See **§H**.
2. **Beta distribution is ongoing**, not a phase. Package identity, install paths, and
   release naming need to be right because they will be used repeatedly.
3. **Fork hygiene and PR-readiness are the same work.** Every hunk kept out of an
   upstream file is both one less rebase conflict *and* one less thing to strip when
   the merge-back proposal is made. This is the happy case: there is no tension
   between maintaining the fork well and eventually upstreaming it.

| Track | Purpose | Which findings matter |
|---|---|---|
| **Track A — merge-back readiness** | Keep the inhibit change shaped so it *can* be proposed upstream | C1–C7, D1–D5, U1–U6, §G |
| **Track B — fork distribution** | Ship maintained test builds; gather multi-op operating evidence | R1–R5, D6–D15 |
| **Track C — fork maintenance** | Keep rebases cheap and safe over time | §H |

**The single biggest lever remains diff size.** The change is currently 54 files /
+5,749 lines. The upstream-relevant core is roughly 18 files / ~900 lines (§G.1).
Everything outside that core is both rebase tax and merge-back friction.

Read §G (merge-back shape) and §H (fork maintenance) before acting on §A–§C.

---

## A. Release blockers — must clear before tagging rc2

### ☑ R1. Windows CI fails: stale `inhibit-spacebar.exe` check

**Severity:** blocker (no Windows package is produced at all)
**Evidence:** `.github/workflows/build-windows.yml:235`

```
test -f "${WORKSPACE}/stage-main/bin/inhibit-spacebar.exe"
```

Commit `eeae769` renamed the target to `inhibit-test-gui`
(`tools/inhibit_spacebar/CMakeLists.txt:9`, `OUTPUT_NAME "inhibit-test-gui"`).

**Impact:** the "Audit staged runtime DLL imports" step fails → no NSIS installer,
no portable ZIP. Most testers are on Windows, so rc2 would ship nothing usable.

**Fix:** change the path to `inhibit-test-gui.exe`. Consider also adding
`test -f .../bin/inhibit-test.exe` so the console tool is covered too.

**Verify:** trigger `Tester packages` workflow (or push a `packages/v*` tag) and
confirm the audit step passes and both `.exe` and `.zip` assets appear.

---

### ☑ R2. `test_http_user_agent` fails

**Severity:** blocker (a red test in a release candidate)
**Evidence:** measured locally —

```
FAIL!  : TestHttpUserAgent::includesApplicationIdentityAndPlatformContext()
         'user_agent.startsWith ("WSJT-X/")' returned FALSE.
         Loc: [tests/test_http_user_agent.cpp(15)]
Totals: 2 passed, 1 failed
```

`revision_utils.cpp:118` now returns `wsjtx-inhibit/…`; `tests/test_http_user_agent.cpp:15`
still asserts the upstream prefix.

**Note:** `http_user_agent()` is consumed only by `Network/Cloudlog.cpp:67,94`.
Changing it buys almost nothing and breaks a test.

**Options:**
- (a) Revert the `http_user_agent()` change, keep `program_title()` branding. Simplest.
- (b) Keep the change and update the test to accept either prefix.

**Decision: (a) — reverted 2026-08-09.** `http_user_agent()` returns `WSJT-X/...`
again. It is read only by `Network/Cloudlog.cpp`, so identifying the fork there bought
nothing while breaking a test and adding a rebase conflict in an upstream file (§H.2).
`program_title()` (window title) and `about.cpp` branding are untouched — those remain
an open decision.

**Verified:** `test_http_user_agent` now 3 passed / 0 failed.

---

### ☑ R3. Windows installer targets the same directory as official WSJT-X

**Severity:** blocker (can destroy a tester's working WSJT-X install)
**Evidence:**
- `CMakeLists.txt:63` — `project (wsjtx …)`
- `CMakeLists.txt:2095` — `set (CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")`
- `CMakeCPackOptions.cmake.in:8` — `CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME}`
- `CMakeCPackOptions.cmake.in:26` — `CPACK_NSIS_INSTALL_ROOT "C:\\WSJT"`

Default install path is therefore `C:\WSJT\wsjtx` — exactly where the official
WSJT-X installer puts stock WSJT-X — and it shares an Add/Remove Programs entry.

**Contradicted by the docs:**
- `INSTALL-WINDOWS.md:58-60` — "Yes. Prefer side-by-side installs."
- `docs/release-notes-wsjtx-inhibit-rc1.md:131` — "Prefer installing beside official WSJT-X"
- `INSTALL.md:52` / `INSTALL-WINDOWS.md:42` — "often under `C:\WSJT\…`"

**Fix:** override `CPACK_PACKAGE_NAME` (or `CPACK_PACKAGE_INSTALL_DIRECTORY`) to
`wsjtx-inhibit` for the NSIS/ZIP path so the default becomes `C:\WSJT\wsjtx-inhibit`.
Check the NSIS uninstall registry key / display name at the same time.

---

**Update 2026-08-09 — chain confirmed from the generated build tree, and refined.**

`CPACK_PROJECT_CONFIG_FILE` is wired up (`CMakeLists.txt:2146-2148`), so
`CMakeCPackOptions.cmake` really is loaded at cpack time and its values win.
Resolved values from `build/`:

| Variable | Value | Source |
|---|---|---|
| `CPACK_PACKAGE_NAME` | `wsjtx` | `CPackConfig.cmake:47` |
| `CPACK_PACKAGE_INSTALL_DIRECTORY` | `wsjtx` | `CMakeCPackOptions.cmake:8` — **overrides** CMake's default `wsjtx 3.0.2-devel`, stripping the version |
| `CPACK_NSIS_INSTALL_ROOT` | `C:\WSJT` | `CMakeCPackOptions.cmake:26` (inside the NSIS block) |
| `CPACK_PACKAGE_INSTALL_REGISTRY_KEY` | `wsjtx 3.0.2-devel` | `CPackConfig.cmake:46` — keeps the version |
| `CPACK_NSIS_DISPLAY_NAME` | `WSJT-X: Digital Modes for Weak Signal Communications in Amateur Radio` | `CMakeCPackOptions.cmake:43` |

**Two distinct problems, and they behave differently:**

1. **Install directory collision — confirmed, and version-independent.** NSIS
   `InstallDir` = `CPACK_NSIS_INSTALL_ROOT\CPACK_PACKAGE_INSTALL_DIRECTORY` =
   **`C:\WSJT\wsjtx`**, identical to stock WSJT-X. Because line 8 strips the version,
   this collides no matter what `-D CPACK_PACKAGE_VERSION` the CI passes.

2. **Add/Remove Programs — a subtler problem than first described.** The registry key
   *retains* the version+channel (`wsjtx 3.0.2-devel`), so it likely does **not**
   overwrite stock's key (`wsjtx 3.0.2`). Result is arguably worse than a clean
   collision: **two uninstall entries, sharing one install directory, showing the
   identical display name** (which is stock's summary string verbatim). Uninstalling
   either one guts the other, and a tester cannot tell them apart in the list.

**Not evidence about R3:** a `cmake --install --prefix …` tree (what
`scripts/windows/Build-Inhibit.ps1` produces, default `C:\WSJT\wsjtx-inhibit`) says
nothing about the installer default. Different mechanism. R3 is strictly about
`cpack -G NSIS` output.

**Verify without installing anything** — from the Windows build dir:

```bat
cpack -G NSIS
findstr /C:"InstallDir" _CPack_Packages\win64\NSIS\project.nsi
```

Expect `InstallDir "C:\WSJT\wsjtx"`. That one line settles R3; no need to run the
installer or risk a real WSJT-X install.

Fixing it also wants `CPACK_NSIS_DISPLAY_NAME` changed — otherwise the entry is
indistinguishable from official WSJT-X in Add/Remove Programs.

---

**RESOLVED 2026-08-09.** Package identity separated from stock WSJT-X, verified by
re-running `cmake -S . -B build` and reading the regenerated CPack files:

| Change | File | Result |
|---|---|---|
| `CPACK_PACKAGE_NAME` → `wsjtx-inhibit` | `CMakeLists.txt` | `CPackConfig.cmake:47` |
| `CPACK_PACKAGE_INSTALL_REGISTRY_KEY` pinned to the package name | `CMakeCPackOptions.cmake.in:13` | version-independent → RCs upgrade one Add/Remove entry in place |
| `CPACK_NSIS_DISPLAY_NAME` → `wsjtx-inhibit (WSJT-X with TX Inhibit)` | `CMakeCPackOptions.cmake.in:50` | no longer stock's summary string |
| `wsjtx` added to Debian `Provides`/`Replaces`/`Conflicts` | `CMakeLists.txt` | verified `CPackConfig.cmake:22` |

Net NSIS result: `InstallDir` = `C:\WSJT\wsjtx-inhibit`. The *project* name stays
`wsjtx`, so runtime data paths (`share/wsjtx`, ...) are untouched.

**Why the Debian hunk was needed:** renaming the package meant the `.deb` no longer
implicitly superseded the distro/official `wsjtx` package while still shipping
`/usr/bin/wsjtx` — a file conflict on install. Declaring `wsjtx` in all three
relationship lists restores the previous behaviour under the new name. (This also
improves on the old state: `INSTALL-LINUX.md` used to warn testers to remove the
distro package by hand.)

**Still unverified:** the actual NSIS `InstallDir` line, which only a Windows `cpack
-G NSIS` can produce. Run the `findstr` check above in the VM to close this out —
expect `C:\WSJT\wsjtx-inhibit` now.

---

### ☑ R4. Settings, logs, and ADIF are shared with stock WSJT-X

**Severity:** blocker (silent cross-contamination of a tester's real station config)
**Evidence:** `main.cpp:142` still does `a.setApplicationName ("WSJT-X")`. There is no
`setOrganizationName`. So `QStandardPaths::DataLocation` (used at `main.cpp:308`)
resolves to the same `%LOCALAPPDATA%\WSJT-X` as official WSJT-X.

Shared: `WSJT-X.ini`, `ALL.TXT`, `wsjtx_log.adi`, log config. The new `EnableTxInhibit`
key (`Configuration.cpp:2631,2846`) is written into the stock config file.

**Interacts with R3:** the docs recommend side-by-side installs, which is exactly the
scenario where shared config bites.

**Options:**
- (a) Change the application name for this fork (cleanest isolation; testers start
  from a fresh config, which is arguably *good* for a test build, but they lose their
  callsign/rig setup and will complain).
- (b) Keep the name and prominently document `--rig-name inhibit`, which produces
  `WSJT-X - inhibit.ini` (`main.cpp:206`). Cheap, reversible, preserves the
  "try it without disturbing my station" promise.

**Decision: (b) — documented 2026-08-09.** `INSTALL-WINDOWS.md` gained a
"Can I keep official WSJT-X installed?" section spelling out that program files are now
separate (R3) but `%LOCALAPPDATA%\WSJT-X\` is **not**, with step-by-step instructions
for adding `--rig-name inhibit` to the Start-menu shortcut. Summarised in `INSTALL.md`
and the rc2 release notes.

Option (a) (separate application name) remains available if testers report confusion;
it is a one-line change in `main.cpp` but costs everyone their existing settings.

**Verified on Linux 2026-08-09** (incidentally, during a GUI smoke test):
`./wsjtx --rig-name inhibit-badge-smoke` created `~/.config/WSJT-X - inhibit-badge-smoke.ini`
and `~/.local/share/WSJT-X - inhibit-badge-smoke/`, leaving the stock `WSJT-X.ini`
untouched. The mechanism is `main.cpp:206` appending to `applicationName()`, which is
platform-independent Qt code — so the Windows behaviour follows, though confirming it
in the VM is still worth a minute.

---

### ☑ R5. Documented asset filenames do not match what CI produces

**Severity:** blocker-ish (testers hunt for a file that isn't there)
**Evidence:** `.github/workflows/build-windows.yml:262` forces

```
-D CPACK_PACKAGE_FILE_NAME="wsjtx-${{ inputs.version }}-win64"
```

producing e.g. `wsjtx-3.0.2-rc2-win64.exe`. Every doc tells testers to look for
`wsjtx-inhibit-…-win64.exe`:
- `INSTALL.md:49,52`
- `INSTALL-WINDOWS.md:20-21`
- `docs/release-notes-wsjtx-inhibit-rc1.md:17-18`

**Fix:** pick one naming scheme and make CI + all four docs agree. Prefer
`wsjtx-inhibit-<version>-win64.exe` since it also fixes the "am I running the right
build?" confusion. Note this couples to R3 if `CPACK_PACKAGE_NAME` changes.

**RESOLVED 2026-08-09.** `build-windows.yml` now produces
`wsjtx-inhibit-<version>-win64.exe`; the signing step's glob and both
`upload-artifact` name/path pairs were updated to match. Verified by configure that
CPack independently derives `wsjtx-inhibit-3.0.2-devel-Linux` /
`wsjtx-inhibit-3.0.2-rc2-Linux`, so the CI override and the CMake default now agree
on the product name rather than diverging.

**Docs still to update** to the new filename: `INSTALL.md:49,52`,
`INSTALL-WINDOWS.md:20-21`, `docs/release-notes-*`. Folded into the rc2 notes task.

---

## B. Code findings

### ☐ C1. Hold timing uses the wall clock, not a monotonic clock

**Severity:** high — can both stick PTT off and release a hold early
**Evidence:** `TxInhibit/TxInhibitGate.cpp:19-22`

```cpp
qint64 TxInhibitGate::now_ms () const
{
  return QDateTime::currentMSecsSinceEpoch ();
}
```

`GateLogic::hold_timeout_at_ms_` (`TxInhibitLogic.hpp:224`) stores an absolute value
in that epoch and compares against it in `inhibited()`.

**Why it matters here specifically:** WSJT-X operators are the most aggressive
time-syncing population in amateur radio — Meinberg NTP, Dimension4, BktTimeSync all
*step* the system clock, often by hundreds of ms to seconds, and often repeatedly.

- Backward step during a hold → hold extends by the step size. PTT stays off with
  no packet to explain it. Looks like a stuck hold.
- Forward step during a hold → hold ends early. **PTT can assert while the priority
  station is still keyed** — the exact failure the product exists to prevent.

**Fix:** switch the time base to `QElapsedTimer` (monotonic). Keep the injectable
`now_ms` parameter shape in `GateLogic` so the unit tests are unaffected.

**Verify:** add a unit test that steps `now_ms` backwards and asserts the hold still
expires on schedule relative to the monotonic base. Manual: hold, then step the system
clock, confirm behaviour.

---

### ☐ C2. Hamlib exceptions can escape a timer/socket slot and abort the app

**Severity:** high — hard crash (`qFatal`) instead of a logged error
**Evidence:**
- `HamlibTransceiver.cpp:278-285` — `error_check()` **throws** on any non-`RIG_OK`.
- `HamlibTransceiver.cpp:415-439` — `apply_physical_ptt()` calls `error_check`.
- `HamlibTransceiver.cpp:375-377` — connected `Qt::DirectConnection` to
  `TxInhibitGate::physicalPtt`.
- `TxInhibitGate.cpp:134-159` — `tick()` (QTimer slot) and `on_udp_ready()`
  (readyRead slot) both call `apply_line()`, which emits `physicalPtt`. Neither has
  a `try`/`catch`.

The stock path did not have this exposure: `TransceiverBase::set` and
`PollingTransceiver::handle_timeout` both wrap their work in `try`/`catch`
(`Transceiver/TransceiverBase.cpp:22,59,213,272`, `PollingTransceiver.cpp:144-186`).

**Failure scenario:** USB serial adapter pulled (or radio powered off) while a hold is
active and `want_tx` is true. The hold expires → `tick()` → `apply_line()` → radiate
true → `rig_set_ptt` fails → throw → unwinds through `QMetaObject::activate` →
`ExceptionCatchingApplication::notify` catches it and calls `qFatal("Aborting")`
(`ExceptionCatchingApplication.hpp:27-46`).

**Fix:** wrap the emit in `TxInhibitGate::apply_line()` (or the body of
`apply_physical_ptt`) in `try`/`catch`, routing the message to `lineError` /
`CAT_TRACE` rather than letting it escape. Teardown already does this correctly at
`HamlibTransceiver.cpp:400-408` — mirror that.

---

### ☐ C3. Badge and telemetry are missing under DXLab / HRD / OmniRig / TCI

**Severity:** high — inhibit works but is completely invisible
**Evidence:**
- `TransceiverFactory.cpp:125,147,170,192` pass `params.enable_tx_inhibit` into the
  inner PTT-only `HamlibTransceiver` for all four wrapper rig types.
- `DXLabSuiteCommanderTransceiver.cpp:62` calls `wrapped_->start(0)`, so the gate
  really does start and really does filter PTT.
- Only `EmulateSplitTransceiver.cpp:24-25` forwards `tx_inhibit_changed` /
  `tx_inhibit_port_bound`. `DXLabSuiteCommanderTransceiver`, `HRDTransceiver`,
  `OmniRigTransceiver`, and `TCITransceiver` hold `wrapped_` but connect nothing.
- `Configuration.cpp:5119-5131` connects to the **outermost** transceiver object.

**Result for those users:** no red badge, no tooltip, no `InhibitStatus` message.
PTT simply stops working with no visible reason. Worst possible failure mode for a
beta tester writing a bug report.

**Fix:** either forward the two signals in the four wrappers (3 lines each, same
pattern as `EmulateSplitTransceiver`), or refuse to enable TX Inhibit for those rig
types and grey the checkbox with an explanatory tooltip.

**Verify:** configure Rig = OmniRig (or DXLab) + PTT = RTS, send a hold, confirm the
badge appears.

---

### ☐ C4. Bind failure / ephemeral fallback is silent — fail-open *and* fail-quiet

**Severity:** high (safety-relevant for an interlock)
**Evidence:**
- `TxInhibitGate.cpp:47-59` — prefers 22372, silently falls back to an ephemeral port,
  emits `lineError` only on total failure.
- `HamlibTransceiver.cpp:382-385` — `lineError` is connected to a lambda whose entire
  body is `CAT_TRACE (msg);`. Nothing operator-visible.
- `mainwindow.cpp:4372-4383` — the only surface for the bound port is `setToolTip` on
  `inhibit_status_label`, which is `hide()`n until a hold is active. So the port is
  discoverable **only while already inhibited**.

**Scenario:** something else holds 22372 (a second WSJT-X instance, a stale process).
The gate binds an ephemeral port. The KEY agent keeps sending to 22372. The station has
**zero** inhibit protection while the operator believes the feature is on. Nothing in
the UI says otherwise.

**Fix:** show the badge in a neutral state whenever the gate is up — e.g. grey
`TX Inhibit: 22372` — and red only when held. Surface a visible warning when the bound
port is not the well-known one, and when bind fails entirely. `Configuration::
tx_inhibit_gate_active()` (see C6) is the natural feed for this.

---

### C4.1 Where to indicate inhibit — design decided 2026-08-09

**A defect the current badge conceals.** `tx_status_label` is WSJT-X's canonical
"what is my transmitter doing" widget (status-bar left) and is already a colour
semaphore: green idle, `#ffff33` transmitting, `#66ffff`/`#ffccff` shorthand,
`#ff0000` above 90% power. It is driven by `m_transmitting`, set by the **sequencer**
at `widgets/mainwindow.cpp:8257` — from the decision to transmit, not from the pin.
The fork's `do_ptt` deliberately calls `update_PTT(intent)`.

So during a hold this label reads `Tx: CQ W1AW FN42` in transmit-yellow **while the
radio is silent**. The primary TX indicator actively misreports, and the current
design answers that by adding a second widget a few pixels away saying the opposite.
Folding inhibit into `tx_status_label` is therefore a **correctness fix**, not a
cosmetic preference.

**Three surfaces, three jobs:**

| # | Surface | Job | Notes |
|---|---|---|---|
| 1 | `tx_status_label` | at-a-glance "am I being held *right now*" | show `INHIBITED` on red instead of `Tx: <msg>` when `want_tx && hold`. Must live inside `guiUpdate()`'s `if (m_transmitting)` branch — that timer rewrites the label unconditionally and would clobber anything set elsewhere. |
| 2 | `inhibit_status_label`, never hidden once bound | persistent **armed** state | dim grey `INH 22372` idle · red `INHIBITED` held · amber `INH ?` on ephemeral/failed bind. The amber row **is** the C4 fix. Also stops the status bar reflowing on every hold, which the present show/hide causes. |
| 3 | `View → TX Inhibit status…` dialog | diagnosis on demand | bound port, holder station, the four counters, time since last packet. All of it already rides on `inhibitChanged` and is currently discarded — the handler uses only the badge string. Right home for "held by ROY-222-SSB". |

**Rejected, with reasons:**

- *Colouring the Enable Tx button* — implies the control is disabled. It is not;
  sequencing continues. Misleading in exactly the way the feature exists to avoid.
- *Window title* — tempting for an unattended go-box over RDP, but it churns on every
  hold/release. Use the `InhibitStatus` UDP telemetry for remote monitoring instead.
- *Waterfall overlay* — separate window, invasive, easy to do badly.

**Also worth doing regardless:** log hold transitions to `ALL.TXT` with timestamps.
Answers "was I held during the QSO I lost?" after the fact, which no live indicator can.

**Flicker:** not a concern in normal operation — the KEY agent's hang is designed to
hold across CW element gaps, so a hold spans a whole transmission. A ~500 ms minimum
dwell is cheap insurance for the case where an agent dies mid-transmission and the
hold timeout fires.

**IMPLEMENTED 2026-08-09 — surface 1 only. Surface 2 was tried and removed.**

**What shipped:** `guiUpdate()` overrides `tx_status_label` in **both** state
branches. Escalation carries the meaning:

| WSJT-X state | Hold | Label |
|---|---|---|
| Monitoring (receive) | none | green `Receiving` *(upstream, unchanged)* |
| Monitoring (receive) | active | pale green `#b3ffb3` **`Inhibit`** — ambient status |
| Transmitting / Tune | none | yellow `Tx: <message>` *(upstream, unchanged)* |
| Transmitting / Tune | active | red `#cc0000` bold **`Inhibit`** — alarm |
| Monitor off, not transmitting | either | blank *(upstream, unchanged)* |

No `Tx:` prefix on the alarm — nothing is being transmitted, and the prefix was a
smaller version of the same lie this fixes. Text is `Inhibit`, not `Inhibited`:
operator preference, one syllable shorter, and it fits the 100 px label.

**Placement bug found in operator testing, and it was instructive.** The first cut
anchored the override on a `tx_status_label.setText (t)` that I took to be the Tx
message. It is not — it is the *Receiving* text inside the `else if (m_monitoring)`
branch. So the override landed in the receive path and never ran during transmit,
i.e. it implemented the case that did not matter and missed the defect entirely.

The symptom set reported from testing pinned it exactly:

| Observation | What it proved |
|---|---|
| Monitor on, hold → shows the text | override is in the monitoring branch |
| Monitor off, hold → blank | third branch (`!m_diskData && !m_tx_watchdog`) clears; override absent |
| Monitor off, **Tune**, hold → `Tx: TUNE` yellow, inhibit not shown, yet the radio *is* held | override missing from the `m_transmitting` branch — Tune runs through it |

**Lesson:** in a 900-line function with several sibling branches setting the same
widget, anchoring a patch on a repeated statement (`setText (t)`) is not enough — the
enclosing branch has to be identified. Verifying by reading the diff would not have
caught this; only running it did.

**Surface 2 (a dedicated always-visible badge) is withdrawn.** Two rounds of visual
testing killed it:

1. *Wording collision.* Both indicators rendered red, bold, reading `INHIBITED`.
   Adjacent, they read as one string — reported as the alarm text being appended to
   the Tx message rather than replacing it. Fixed by making the badge show only
   `INH <port>`, colour carrying state.
2. *Position and hiding.* `config_label` between them is hidden by default, so the
   badge abutted `mode_label` — `#ff6699` for FT8, `#ff6666` for MSK144 — producing
   three touching red boxes. Separately, `showStatusMessage()` calls
   `statusBar()->showMessage(msg, 5000)`, which hides every **non**-permanent widget;
   the badge would have vanished for five seconds at a time. Moving it to the
   permanent right-hand group fixed both.
3. *Rejected anyway.* Operator verdict: the extra box **disrupts the spacing of the
   whole status-bar line** and is not worth its width for something that is
   uninteresting most of the time. Removed entirely.

**How the C4 signal survives without a widget** — `update_inhibit_status()`:

| Channel | Carries | Cost |
|---|---|---|
| Tooltip on `tx_status_label` | current state, bound port, holding station | none |
| One-shot `showStatusMessage()` | "enabled but no port bound — NOT protected", or "listening on N, not 22372" | none (transient) |

The status message fires only on a *change* of reachability (guarded by
`m_tx_inhibit_warned` / `m_tx_inhibit_warned_port`), so it warns once per transition
and cannot nag.

**Honest limitation:** a tooltip is not discoverable, and a 5-second message can be
missed. The fail-silent hole in C4 is *narrowed*, not closed. Closing it properly needs
a surface that persists — which is exactly what was rejected — so the remaining option
is the surface-3 diagnostics dialog, or accepting that an operator who wants certainty
runs a test hold. Recorded rather than argued.

**Known residual:** a rig closed by CAT *failure* zeroes the port without emitting, so
the tooltip can lag until the next signal. Fixing it needs a new signal on
`Configuration`, i.e. more upstream-owned surface (§H.1).

Verified: builds clean and starts.

**On-air-adjacent verification, Linux, 2026-08-09.** With a hold latched on
(`inhibit-test`, `~`), an **RX → Tune → RX** cycle on real hardware behaved correctly:
the radio did not key while `want_tx` was asserted via Tune, and PTT was not left
stranded on the way back to receive. That exercises the pin filter through a state
transition rather than a static hold, and covers the no-stuck-PTT path.

**Label confirmed red `Inhibit` during that Tune.** The branch fix landed: the label
now reports the transmitter rather than the sequencer, in the very case an earlier
build got wrong. C4.1 surface 1 is verified end to end on Linux — receive, transmit,
and Tune — against a real radio.

What remains for this finding is Windows-side only, and none of it is about the label
logic: the NSIS `InstallDir` line, the firewall prompt, and the port-occupied case.

**Recommended order:** 1 and 2 together (same edit region, and 1 is a defect fix).
3 only when someone needs to debug an agent — the UDP telemetry already exists, so an
external tool can serve that need first and prove whether an in-app dialog is wanted.

---

### ☐ C5. `ptt_on_` is now set unconditionally — unintended stock-path change

**Severity:** medium (deviation from upstream in shared code; maintainers will flag it)
**Evidence:** `HamlibTransceiver.cpp:1380` hoists `ptt_on_ = on;` above the
`RIG_PTT_NONE != m_->rig_->state.pttport.type.ptt` guard that upstream had inside both
branches of `do_ptt`.

For a rig with no PTT port, `ptt_on_` now becomes true, which enables the PWR/SWR
polling block at `HamlibTransceiver.cpp:1326` that upstream never reached. This happens
**whether or not TX Inhibit is enabled**.

**Fix:** restore the guard, or scope the unconditional assignment to the
`inhibit_gate_` branch only.

---

### ☐ C6. Dead code: `Configuration::tx_inhibit_gate_active()`

**Severity:** low
**Evidence:** declared `Configuration.hpp:398`, defined `Configuration.cpp:1266`,
called nowhere in the tree.

**Fix:** wire it into the "gate is listening" badge (C4) or remove it.

---

### ☐ C7. UDP socket accepts holds from anyone, and the firewall prompt is undocumented

**Severity:** medium (design choice, but undocumented)
**Evidence:** `TxInhibitGate.cpp:47-49` binds `QHostAddress::AnyIPv4` with
`ShareAddress | ReuseAddressHint`. No source allowlist, no authentication, no
`band` filtering (`TxInhibitLogic.hpp:56` notes band is informational only).

Any host that can reach the machine can hold a station off the air with one ~60-byte
packet. That is defensible on a trusted multi-op LAN — but it is not stated in any
operator-facing doc, and Windows testers will get a Windows Defender Firewall dialog on
first run that no doc prepares them for.

Also: `ShareAddress` means multiple WSJT-X instances on one host share 22372 and which
one receives a given datagram is OS-dependent. That caveat exists only as a code comment
at `TxInhibitGate.cpp:44-46`.

**Fix (docs, minimum):** add a security note to `docs/TX_INHIBIT.md §4` and firewall
guidance + the one-instance-per-host caveat to `INSTALL-WINDOWS.md`.
**Fix (code, optional / later):** optional sender allowlist or shared secret.

---

## C. Documentation findings

### Maintainer lens

The design writing is genuinely strong — `docs/TX_INHIBIT.md` is more rigorous than
most upstream feature docs, and `UPSTREAM.md` pinning the baseline commit is exactly
right. Push-backs:

#### ☑ D1. `NetworkMessage::InhibitStatus` = 17 — *corrected: this is fine*

**Original finding was too harsh.** Upstream's own protocol rules
(`Network/NetworkMessage.hpp:76-101`) state explicitly:

> 1. New message types may be added to the protocol in the future, third-party
>    applications and WSJT-X shall ignore silently any message types they do not
>    recognize.
>
> Note that these rules are unrelated to the schema number above […] New message types
> and extra fields in existing messages can and will be added without any change in
> schema number.

So adding `InhibitStatus` as an Out message without a schema bump is **exactly the
documented procedure**. Third-party apps that break on it are violating a rule that
predates this change.

**What remains, and it is small:** the *number* 17 is upstream's to assign. In a fork it
is a squatted value; in a PR it is a proposal, which is the correct way to obtain it.
Expect a reviewer to confirm or renumber it. No pre-emptive action needed beyond saying
so in the PR description.

**Residual risk to note in the PR:** in practice several popular consumers do not
robustly skip unknown types. Worth offering to gate `InhibitStatus` emission behind the
same Settings checkbox (it already is — `mainwindow.cpp:4386-4392` only fires inside
the `tx_inhibit_changed` handler, which only fires when the gate exists).

#### ☐ D2. Branding is interleaved with the feature

`revision_utils.cpp`, `widgets/about.cpp`, `widgets/about.ui`, `widgets/mainwindow.ui`,
and the shortcut table in `widgets/mainwindow.cpp:5202` all change identity strings.
Fine for a fork, but the TX Inhibit feature is not extractable as a clean patch if an
upstream PR is ever the goal.

**Fix:** separate branding into its own commit, clearly labelled.

#### ☐ D3. Self-declared unfinished naming in the design authority doc

`docs/TX_INHIBIT.md:503-504`: *"Identifier names in code may still say
gate/hold/block/intent; align in a later code pass."* True (`GateLogic`,
`line_inhibited`, `tx_inhibit_gate_active`, `physicalPtt`), but leaving the apology in
the spec reads as unfinished work to a reviewer. Either do the rename or drop the note.

#### ☑ D4. CI never runs on this branch

`.github/workflows/ci.yml:5-8` triggers on push/PR to `develop`. The repo lives on
`main`. Nothing has been automatically build-checked — which is why R1 and R2 survived.

**Fix:** add `main` to the trigger branches.

#### ☐ D5. The wire contract is buried

`docs/TX_INHIBIT.md` §3–§4 is the interoperability contract someone needs to write an
independent KEY agent. It sits behind ~170 lines of operator wiring advice.

**Fix:** split into `docs/TX_INHIBIT_PROTOCOL.md`, leave a pointer.

---

### Tester lens (Windows-first)

#### ☑ D6. The README that actually ships in the portable ZIP is wrong

`scripts/windows/Package-PortableZip.ps1:46`:

> With wsjtx running, run bin\inhibit-spacebar.exe and press Spacebar to hold/release.

Wrong binary name **and** wrong key. This is the first thing a ZIP tester reads.

#### ☑ D7. Spacebar-vs-grave is inconsistent across the docs

The C++ tools use grave/backtick and the docs shout "**not Space**" — but these still
say Spacebar:
- `INSTALL.md:5, 143, 191, 199, 204, 215`
- `docs/TX_INHIBIT.md:6, 422`

`tools/send_inhibit_hold.py` actually uses grave (`VK_OEM_3` / `KEY_GRAVE`, see its
lines 6, 82-83, 152) — its internal helpers are still *named* `_space_level_win` /
`space_down`, which is where the doc confusion came from.

#### ☑ D8. Two stale spacebar tools are still in the tree

- `tools/Run-InhibitSpacebar.cmd` launches …
- `tools/inhibit_spacebar_gui.py` — a tkinter GUI that genuinely binds
  `<KeyPress-space>` (lines 200-201) and titles itself "Spacebar TX Inhibit Tester"
  (line 64).

Superseded by the native `inhibit-test-gui.exe`, referenced by no doc, and teaches the
wrong key.

**Fix:** delete both.

**DONE 2026-08-09.** Both removed. Confirmed beforehand that nothing in `CMakeLists.txt`,
`scripts/`, or `.github/` referenced or packaged them, and that the `.cmd` existed only
to launch the `.py`. Recoverable from `be43cd2` if ever needed.

**Consequence, accepted:** Linux testers now have the console `inhibit-test` only —
`inhibit-test-gui` is Windows-only (native Win32). Revisit if Linux testers ask for a
GUI; the right answer then is a small Qt GUI sharing `inhibit-test`'s state machine,
not resurrecting a tkinter tool bound to the wrong key.

#### ☑ D9. Seven broken markdown anchors

Two distinct targets, found by an automated link/anchor sweep of all 14 `.md` files:

| Broken link | In |
|---|---|
| `docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr-what-operators-actually-do` → heading is now `### Shared USB CAT + RTS/DTR`, so the anchor is `#shared-usb-cat--rtsdtr` | `INSTALL.md:76`, `INSTALL-WINDOWS.md:104,150`, `INSTALL-LINUX.md:149,188`, `docs/WIMS_TX_INHIBIT.md:12` |
| `../INSTALL.md#6-test-inhibit-with-the-spacebar-helper` → section is now "Test TX Inhibit with the KEY helper (`inhibit-test`)" | `docs/TX_INHIBIT.md:477` |

**Suggestion:** add a CI job that runs a markdown link checker so these don't recur.

#### ☑ D10. Missing operator-facing topics

None of these appear anywhere, and every Windows tester will hit at least the first:

1. **Windows Firewall prompt** on first launch with TX Inhibit enabled (UDP bind).
2. **What "nothing happens" means** — fail-open. No agent → no hold → normal PTT.
   State it plainly so testers don't report "inhibit doesn't work" when it's idle.
3. **One WSJT-X instance per host** can reliably receive on 22372 (see C7).
4. **How to confirm the gate is listening** at all (currently impossible — see C4).

#### ☑ D11. Tag scheme doesn't match reality

`README.INHIBIT.md:37-44` documents `build/v3.0.2-rc1` and `packages/v3.0.2-dev1`.
The actual shipped tag is `wsjtx-inhibit-rc1`, which matches neither trigger
(`tester-packages.yml:22` wants `packages/v*`) and therefore fired no workflow — the
rc1 release was effectively hand-built.

**Fix:** decide the rc2 tag scheme, make `README.INHIBIT.md` match, and use it.

**Answered 2026-08-09 — how upstream actually does it.** The product name never
changes across release stages; the stage lives in the *version string*, set by
`set_build_type()` (`CMake/Modules/set_build_type.cmake`) from two cache variables:

| Channel | `BUILD_TYPE_REVISION` | `wsjtx_VERSION` |
|---|---|---|
| DEVEL (default) | `-devel` | `3.0.2-devel` |
| RC *n* | `-rc<n>` | `3.0.2-rc2` |
| GA | *(empty)* | `3.0.2` |

`release.yml:24-35` parses the tag: `build/vX.Y.Z` → GA, `build/vX.Y.Z-rcN` → RC with
*n* extracted, anything else is a hard error. GA-only steps (source tarball, public
repo push, public Release) are gated off RC. Historic evidence in `Release_Notes.txt`:
`WSJT-X 2.7.0-rc8`, `3.0.0-rc1`.

**So: do NOT name the product `wsjtx-inhibit-rc2`.** Product identity
(`wsjtx-inhibit`, constant) and release stage (`rc2`, varying) are separate knobs.
Baking the stage into the name gives a new install directory and a new Add/Remove
entry per RC, with no upgrade path — worse than the status quo.

**Recommended for rc2:** tag `build/v3.0.2-rc2`. It is the only tag that sets the RC
channel end-to-end, and `release.yml` already validates it. `packages/v3.0.2-rc2` is
faster but was only correct after the D15 fix.

**Why rc1 was hand-built:** the tag `wsjtx-inhibit-rc1` matched neither `build/v*` nor
`packages/v*`, so no workflow fired.

#### ☑ D12. `.gitignore` does not cover `build/`

The only thing dirtying `git status` today.

#### ☑ D13. `docs/BUILDING.md` will send a Windows VM down a dead end

Found while preparing a VM build. The Windows half of this document appears never to
have been executed.

| `docs/BUILDING.md` says | Reality |
|---|---|
| line 12 — "C++17 compiler … MSVC 2019+ (Windows)" | Windows builds use **MSYS2 MINGW64** (`build-windows.yml:31,46-48`, `scripts/windows/Build-Stage.ps1:4`). And `TxInhibitLogic.hpp:33` records the project builds `--std=gnu++11 -Werror` — commit `a3d0802` was specifically "MinGW C++11". Both halves of the claim are wrong. |
| lines 62-71 — Visual Studio + Ninja + `msvc2019_64` Qt kit | No MSVC path exists in CI or the stage scripts |
| line 25 — Hamlib `4.7.0` | CI pins **4.7.1** (`ci.yml`, `tester-packages.yml`). Upstream `CONTRIBUTING.md:47` says to read the current value from `ci.yml` rather than hardcoding it |
| line 11 — CMake ≥ 3.7.2 | Upstream `CONTRIBUTING.md:21` says 3.16+ |
| line 80 — `git tag build/v3.0.2-rc1` | Same stale tag scheme as D11 |

**Fix:** rewrite the Windows section around MSYS2 MINGW64 (or delete it and point at
upstream `CONTRIBUTING.md` + `scripts/windows/`), and stop hardcoding the Hamlib version.

**Update (2026-08-09):** largely superseded by the untracked `docs/WINDOWS_DEV.md` +
`scripts/windows/Build-Inhibit.ps1`, which document the real MinGW + Qt5 path. Simplest
resolution is to **delete `docs/BUILDING.md`'s Windows section** and point at
`WINDOWS_DEV.md` rather than maintain two accounts of the same build.

**Watch out:** `Build-Inhibit.ps1` uses a `cmake --install` prefix of
`C:\WSJT\wsjtx-inhibit`, which is the R3-correct location — but that is the *install*
prefix, not `CPACK_PACKAGE_INSTALL_DIRECTORY`. A `cpack -G NSIS` from the same build
tree will still default to `C:\WSJT\wsjtx` and collide with stock WSJT-X. **R3 is not
fixed by the build script** — test the NSIS installer specifically.

**Keep:** the "Verify TX Inhibit after build" smoke test at lines 87-99 is good — the
`nc -u` one-liner is exactly the two-minute reviewer test the PR description needs
(§G.6). Payload verified valid against the parser (`ttl_ms` 2000 is inside the
100–30000 range). Add a PowerShell equivalent: Windows has no `nc`.

#### ☐ D16. Linux `input`-group instructions push testers toward breaking their session

`tools/README-INHIBIT-TESTER.md`, `INSTALL.md`, and `docs/TX_INHIBIT.md` all say to run
`usermod -aG input $USER` "then full log out/in". Correct, but presented as the only
option — so the natural shortcut is `su -l $USER`, which **scrubs the environment**
(`DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, `DBUS_SESSION_BUS_ADDRESS` are set by
the graphical login via PAM/systemd, not by any dotfile).

Hit on this machine 2026-08-09: `./wsjtx` failed with *"could not connect to display"*
(empty `DISPLAY`). Worse than it looks — the same shell also loses `XDG_RUNTIME_DIR`,
so PipeWire is unreachable and WSJT-X would have started with **no audio devices**,
sending the tester chasing a phantom sound-card bug.

**Fix the docs to offer the environment-preserving alternative:**

```bash
sg input -c './bin/inhibit-test --host 127.0.0.1 --port 22372'
```

Verified: keeps `DISPLAY=:0` and `XDG_RUNTIME_DIR`, gains the group, `/dev/input/event0`
readable. `newgrp input` is the interactive equivalent.

**Also worth stating:** `wsjtx` itself never needs the `input` group — only
`inhibit-test` does. Nothing in the docs currently separates the two, which is why the
group elevation gets applied to the wrong program.

#### ☑ D15. `tester-packages.yml` hardcoded DEVEL, so the version string lied

**Found 2026-08-09** while answering "how does WSJT-X name release candidates."

`tester-packages.yml` passed `release_channel: "DEVEL"` literally for both the Linux
and Windows jobs, and never passed `rc_number` — while the artifact *filename* came
from the tag. So `packages/v3.0.2-rc2` produced a file named
`...-3.0.2-rc2-win64.exe` whose compiled-in version, shown in Help → About, read
**`3.0.2-devel`**. A tester reporting "I'm on rc2" would be reading a different string
from the one on the file they downloaded.

**Fixed:** the `prepare` job now derives `release_channel` / `rc_number` from the
version suffix (`-rcN` → RC *n*, anything else → DEVEL, since tester packages always
publish as prereleases) and passes both through. `build-linux.yml` and
`build-windows.yml` already accepted and consumed both inputs
(`build-linux.yml:116-117`, `build-windows.yml:188-189`) — only the caller was wrong.

**Verified** by configuring with `-DWSJT_RELEASE_CHANNEL=RC -DWSJT_RC_NUMBER=2`:
CMake reported `Building wsjtx v3.0.2-rc2` and CPack derived
`wsjtx-inhibit-3.0.2-rc2-Linux`. Build tree restored to DEVEL afterwards.

**Note:** real GA/RC releases should still go through `release.yml` (`build/v*`),
which validates the tag format strictly and gates the GA-only steps. `packages/v*` is
the faster Windows+Linux path.

#### ☑ D14. `scripts/windows/Build-Stage.ps1` has stale defaults and an odd hard dependency

- `SourceDir` defaults to `C:\src\wsjtx-wims` (line 11) — pre-rename path.
- Line 27 **throws** if `OmniRig.exe` is not installed. A build script that refuses to
  run without a third-party rig-control app is surprising; if this is a genuine build
  requirement it should be documented, and if it is a runtime convenience it should be
  a warning.

---

## D. Track B plan — beta distribution from the fork

**Purpose:** keep shipping test builds so the upstream PR can cite real multi-op
operating experience across platforms. None of this ships in the PR.

Sequenced so that each phase leaves the tree in a taggable-or-better state.

### Phase 1 — unblock the build (mechanical, ~1 sitting)

- [x] R1 — fix `inhibit-spacebar.exe` → `inhibit-test-gui.exe` in `build-windows.yml`
- [x] R2 — decide (a) or (b), fix the UA test
- [x] D4 — add `main` to `ci.yml` triggers
- [x] D12 — add `build/` to `.gitignore`
- [ ] Run the `Tester packages` workflow to prove Windows + Linux artifacts build

**Exit criterion:** green CI on `main`, Windows `.exe` and `.zip` produced.

### Phase 2 — packaging identity (needs a decision, then mechanical)

- [x] R3 — `CPACK_PACKAGE_NAME` → `wsjtx-inhibit`; verify default install path
- [x] R4 — decide app-name isolation vs documented `--rig-name`; implement
- [x] R5 — align asset filenames between CI and all four docs
- [ ] Test in a VM that already has official WSJT-X installed

**Exit criterion:** installing rc2 on a machine with stock WSJT-X leaves stock
untouched — binaries, settings, and logs.

### Phase 3 — doc sweep (mechanical, no decisions)

- [x] D6 — fix the portable-ZIP README text
- [x] D7 — purge remaining "Spacebar" language from `INSTALL.md` and `docs/TX_INHIBIT.md`
- [x] D8 — delete `Run-InhibitSpacebar.cmd` and `inhibit_spacebar_gui.py`
- [x] D9 — fix all 7 anchors (verified 0 broken across 16 files); link-checker CI job still open
- [x] D10 — add firewall, fail-open, one-instance, and "is it listening?" sections
- [x] D11 — settle the tag scheme, update `README.INHIBIT.md`
- [x] Write `docs/release-notes-wsjtx-inhibit-rc2.md` (rc1 notes are a good template)

**Exit criterion:** a tester can go from the Releases page to a confirmed
`TX INHIBITED` badge without hitting a wrong filename, wrong key, or dead link.

### Phase 4 — code correctness (the substantive work)

Priority order — C2 and C3 are the two most worth having before more people put this
on the air; C1 is the one most likely to generate a mystifying bug report.

- [ ] C2 — wrap the emit path so Hamlib exceptions can't reach the event loop
- [ ] C3 — forward inhibit signals in the DXLab/HRD/OmniRig/TCI wrappers
      (or disable the feature for those rig types)
- [ ] C1 — move to a monotonic time base; add a clock-step unit test
- [ ] C4 — visible "gate is listening / port N" state; warn on non-standard port
- [ ] C5 — restore the `RIG_PTT_NONE` guard around `ptt_on_`
- [ ] C6 — wire up or delete `tx_inhibit_gate_active()`

**Exit criterion:** no silent-failure path left where the operator believes TX Inhibit
is protecting them and it isn't.

**Note:** former "Phase 5 — maintainer-facing polish" has moved to §G, where it is the
main event rather than an afterthought.

---

## E. Open questions to settle

**Track A — decide these before writing PR code (§G.5 Phase A1). These change the design.**

1. **Q1 / transport:** separate listener on 22372, or an inbound message on the existing
   WSJT-X UDP protocol? Would you accept the latter if upstream asks for it? *(§G.3 Q1)*
2. **PTT methods:** lift the RTS/DTR-only restriction to cover CAT PTT? `apply_physical_ptt`
   already supports it, and CAT PTT is what most modern rigs use. *(§G.2)*
3. **C4 / failure policy:** is fail-open right for an interlock, or should a
   configured-but-not-listening gate refuse to transmit? Needed for §G.3 Q3.
4. **C7 / security posture:** ship with an opt-in source allowlist, or document
   "firewall this port" and accept open receipt? Needed for §G.3 Q4.
5. **C3:** forward the signals in the four wrappers, or restrict TX Inhibit to direct
   Hamlib CAT rigs and say so in the UI? *(forwarding is the better answer for a PR)*

**Track B — decide these before tagging rc2.**

6. **R2:** revert the User-Agent change, or keep it and update the test?
   *(Track A resolves this by dropping the branding entirely — reverting is cheapest.)*
7. **R4:** separate application name (clean isolation, testers lose their config) or
   documented `--rig-name` (preserves config, relies on the tester reading)?
8. **D11:** what is the rc2 tag? `packages/v3.0.2-rc2` fires the tester-packages
   workflow; `wsjtx-inhibit-rc2` fires nothing.

---

## F. What was verified vs. what is inferred

**Measured directly:**
- `test_tx_inhibit_logic`: 12/12 pass.
- `test_http_user_agent`: 1 failure, output quoted in R2.
- Markdown link/anchor sweep across all 14 `.md` files: 7 broken anchors, 0 missing files.
- All `new HamlibTransceiver` call sites checked — no arg-count hazard from the new
  `enable_tx_inhibit` parameter (the four wrapper sites all pass it explicitly).
- Signal forwarding: confirmed by grep that only `EmulateSplitTransceiver` connects the
  wrapped transceiver's signals.

**Verified by reading Hamlib 4.7.1 source (`~/hamlib-prefix/src`, tag `4.7.1`):**
- `win32_serial_open()` guards the `\\.\` prefix; `check_com_port_in_use()` does not.
- `serial_open()` (CAT) runs the unguarded check; `ser_open()` (separate PTT) does not.
- `git tag --contains 028e8f38c` → present in 4.6.1 through 4.7.2.
- Port names reach WSJT-X as bare `COMn` (`Configuration.cpp:5790-5801`).

**Inferred from code reading, not executed:**
- C1 clock-step behaviour (reasoned from the absolute-epoch comparison).
- C2 crash path (reasoned from `error_check` throwing + no `try` in the slot chain +
  `ExceptionCatchingApplication::notify` calling `qFatal`).
- R3 NSIS default path (derived from the CPack variable chain, not from running the
  installer). **Worth confirming in a VM before acting.**

**Executed on Windows (2026-08-09) — see U6 update and
`docs/verification/u5-u6-windows-2026-08-09/REPORT.md`:**
- U5 / U6 hard-fail claim (`CreateFileA` on double-prefixed path → `-RIG_EIO`) did
  **not** reproduce on Win 10.0.26200 + Hamlib 4.7.1 + motherboard COM1. Double
  prefix is real in source; CreateFile still opens.

---

## G. Track A — upstream pull request

### G.0 What upstream's process actually is

From `CONTRIBUTING.md` (this tree carries upstream's copy):

- Fork `WSJTX/wsjtx`, branch, PR **to `master`** on the public repo.
- The public repo only receives code at release time; a team member ports an accepted
  PR into the private `wsjtx-internal` repo, where it enters normal development and CI.
  **Your change reaches the public repo at the next tagged release**, not at merge.
- *"One PR per logical change. Don't bundle unrelated fixes."*
- *"Include in the PR description: what the change does, why, which platforms you
  tested on, and related issue numbers."*
- Core team is seven volunteers. *"PRs may take days or weeks to review."*

**Process implication:** you get one shot at a first impression, review is
asynchronous and slow, and you cannot iterate quickly against their CI. That argues
strongly for landing a small, obviously-correct, well-argued patch rather than a large
one you plan to defend in comments.

**Suggested pre-step:** open an Issue on `WSJTX/wsjtx` describing the multi-op
same-band interlock problem and the proposed approach, *before* the PR. Cheap, gets the
design objections (§G.3) surfaced early, and gives the PR an issue number to cite as
`CONTRIBUTING.md` requests. If there is genuinely support for the feature, this is where
it will show up — and if the answer is "we'd want it done differently," you learn that
before rewriting 900 lines twice.

---

### G.1 Scope — what goes in the PR

**In (~18 files, ~900 lines):**

| File | Note |
|---|---|
| `TxInhibit/TxInhibitLogic.hpp` | new; pure, unit-tested |
| `TxInhibit/TxInhibitGate.{hpp,cpp}` | new |
| `Transceiver/HamlibTransceiver.{hpp,cpp}` | the pin filter |
| `Transceiver/Transceiver.hpp` | two signals |
| `Transceiver/TransceiverFactory.{hpp,cpp}` | `enable_tx_inhibit` in ParameterPack |
| `Transceiver/EmulateSplitTransceiver.cpp` + the 4 wrappers from C3 | signal forwarding |
| `Configuration.{hpp,cpp,ui}` | the Settings checkbox |
| `widgets/mainwindow.{cpp,h}` | status badge |
| `Network/NetworkMessage.hpp`, `Network/MessageClient.{hpp,cpp}` | `InhibitStatus` |
| `CMakeLists.txt` | source list only — **not** the SerialPort line, see U1 |
| `tests/test_tx_inhibit_logic.cpp`, `tests/CMakeLists.txt` | keep; upstream values tests |
| `docs/TX_INHIBIT.md` | trimmed — see G.2 |

**Out — must not appear in the PR:**

| Excluded | Why |
|---|---|
| `revision_utils.cpp`, `widgets/about.{cpp,ui}`, `widgets/mainwindow.ui` title, the `Ctrl+F1` shortcut text | fork branding; also fixes R2 for free |
| `INSTALL.md`, `INSTALL-WINDOWS.md`, `INSTALL-LINUX.md`, `INSTALL` | fork distribution docs |
| `README.INHIBIT.md`, `README.WIMS.md`, `README.md` banner, `UPSTREAM.md` | fork identity |
| `docs/WIMS_TX_INHIBIT.md`, `docs/release-notes-*.md`, this file | fork process |
| `docs/BUILDING.md`, `docs/SUPERBUILD.md` | duplicate upstream `CONTRIBUTING.md` |
| `scripts/windows/`, `scripts/linux/`, `CMakeCPackOptions.cmake.in` | fork packaging |
| `.github/workflows/*` | fork CI |
| `tools/` (all of it) | see G.2 |
| `CMakeLists_wsjtx.txt` | not an upstream file |

**Net effect:** R3, R4, R5, D6, D7, D8, D9, D10, D11, D12 all drop out of the PR
entirely. They remain live for Track B only.

---

### G.2 Judgement calls on the boundary

**`tools/inhibit-test` (1,005 lines of platform keyboard code).** Cut from the initial
PR. Upstream does ship `UDPExamples/`, so a reference KEY agent is not unprecedented —
but 1,000 lines of `/dev/input` and Win32 raw-key handling is a large, separately
reviewable, separately maintainable surface that has nothing to do with whether TX
Inhibit is correct. Offer it as a **follow-up PR** if the feature lands. Same for
`send_inhibit_hold.py`.

*Consequence:* the PR needs some other way for a reviewer to exercise the feature.
Include a ~30-line `UDPExamples`-style sender, or just document the two JSON payloads
and a `netcat`/PowerShell one-liner in the PR description. The latter is probably
enough and costs nothing.

**`docs/TX_INHIBIT.md`.** Keep, but trim hard. Sections §3.3–§3.5 (Morse timing tables,
WPM-vs-hang arithmetic, distinguishability analysis) are KEY-agent implementation
guidance, not WSJT-X behaviour — they describe software that is not in the PR. Cut to:
glossary, the core equation, station behaviour, the wire protocol, and the code map.
Roughly 500 lines → ~180. This also resolves D5 (split the protocol out) by making the
whole remaining document *be* the protocol + behaviour spec.

**The RTS/DTR-only restriction — revisit before submitting.** `apply_physical_ptt`
(`HamlibTransceiver.cpp:415-439`) calls `rig_set_ptt`, which handles **every** PTT type
including CAT. The gate is PTT-method-agnostic; the restriction is imposed artificially
in two places (`HamlibTransceiver.cpp:328-330,347-349` and `Configuration.cpp:3306-3308`).
Since CAT PTT is what most modern Icom/Yaesu users actually run, "RTS/DTR only" halves
the audience for no implementation reason I can find. Expect *"why only RTS/DTR?"* to be
the second question asked, and to have no good answer once a reviewer reads
`apply_physical_ptt`. Either lift the restriction or be ready to state the reason
crisply. **This is a design decision to make before writing the PR, not during review.**

---

### G.3 The design objections to pre-answer

These are the questions a WSJT-X core developer will ask. Each needs a paragraph ready
in the PR description — not improvised in a comment thread three weeks later.

**Q1. Why a second UDP listener on port 22372 instead of adding an inbound message to
the existing WSJT-X UDP protocol (port 2237)?**

This is the central objection. The existing protocol already has inbound messages,
including `HaltTx` (type 8, In), and an established `MessageServer`/`MessageClient`
pair. A reviewer will reasonably ask why this needs its own port and its own encoding.

The strongest honest answer, in order:
1. **The existing UDP server slot is single-occupancy and usually already taken.**
   WSJT-X connects to *one* configured server (Settings → Reporting). In practice that
   is JTAlert, GridTracker, or N1MM. A multi-op interlock cannot compete for the slot,
   and requiring operators to give it up is a non-starter.
2. **The topology is inverted.** The existing protocol is one WSJT-X → one server. A
   KEY agent is one agent → *many* WSJT-X stations, pushing unsolicited, with no prior
   handshake and no per-station configuration on the agent side.
3. **`HaltTx` has the wrong semantics.** It aborts the QSO sequence. TX Inhibit must
   leave sequencing and audio untouched — that is the entire point of the feature and
   the thing that distinguishes it from every existing mechanism.

Weak answers to avoid: "JSON is easier" (upstream's binary protocol is fine and
already implemented), "lower latency" (not materially true at these packet sizes).

**Be prepared for the counter-proposal** that this should be an inbound message on the
existing protocol anyway, with the server-slot problem solved separately. Decide in
advance whether you would accept that outcome — it is a large rewrite of the transport
but leaves the pin-filter core intact.

**Q2. Why gate at the Hamlib PTT pin rather than in the sequencer?**

Gating in the sequencer (refusing to start a transmission) is architecturally cleaner
and is where a reviewer's instinct will go. The answer is latency granularity: a
sequencer-level gate can only act at T/R boundaries, so a priority station keying
mid-cycle would not be protected for up to 15 seconds. The feature's value is
sub-100 ms response *during* an active transmission.

**Weakness to acknowledge up front, not hide:** the pin-level gate desynchronises
software PTT state from the physical line, which is exactly why `do_poll` must skip
`GET_PTT` while the gate is active (`HamlibTransceiver.cpp:1309-1312`). Name this
trade-off yourself in the PR. A reviewer who discovers it unaided will trust the rest
of the patch less.

**Q3. What happens when it fails?**

Currently: fail-open and fail-silent (C4). For an interlock, a reviewer will ask what
happens when the port is taken, the agent dies, or the network drops. You need a
defensible, *documented*, and *visible* answer for each. C4 is therefore not just a
usability fix — it is a prerequisite for the PR being credible.

**Q4. What is the security model?**

Any host that can reach the machine can hold a station off the air (C7). On a trusted
multi-op LAN that is fine; upstream ships to the whole world. Expect to be asked for at
least an opt-in source restriction, or a clear statement that the listener should be
firewalled. Have a position.

**Q5. Which platforms did you test on?**

`CONTRIBUTING.md` asks directly. macOS is currently untested. This must be answered
honestly and specifically — which is what Track B exists to produce.

---

### G.4 Additional findings specific to the upstream path

#### ⊘ U1. `Qt5::SerialPort` on `wsjt_qt` — *corrected: NOT dead weight*

**Verified:** no file under `TxInhibit/` or `Transceiver/` references `QSerialPort` or
`serialport`. Upstream links `Qt5::SerialPort` to the `wsjtx` target only
(`CMakeLists.txt:1759`); this change adds it to the shared `wsjt_qt` static library
(`CMakeLists.txt:1619`, and `CMakeLists_wsjtx.txt:1552`).

**Correction 2026-08-09 — the original finding was wrong.** `Configuration.cpp` *is*
part of `wsjt_qt` (`CMakeLists.txt:344`) and includes `<QSerialPortInfo>` at line 181.
The dependency is real, not leftover.

Why upstream builds without it: `wsjt_qt` is a **static** library, so unresolved symbols
are not diagnosed until the final link, and upstream links `Qt5::SerialPort` into the
`wsjtx` executable (`CMakeLists.txt:1759`). Adding it to `wsjt_qt` makes the dependency
explicit and propagates it to every consumer — arguably more correct than upstream,
but a behaviour change to a shared target either way.

**Revised action:** for the merge-back PR, drop the hunk to match upstream *unless* a
consumer actually fails to link. Verify on MinGW (which is where commit `a3d0802` added
it) before deciding. Not "dead weight" — do not remove it casually.

#### ☐ U2. Header guard and naming conventions — check, don't assume

`CONTRIBUTING.md:138` says *"methods use `camelCase`. Header guards use `FILENAME_HPP_`
format."* The **actual** codebase uses `snake_case` methods (`do_ptt`, `update_PTT`)
and `NETWORK_MESSAGE_HPP__` guards. The new code follows the real codebase
(`TX_INHIBIT_GATE_HPP__`, `start_listening`), which is correct — but `physicalPtt`,
`inhibitChanged`, `portBound`, and `lineError` in `TxInhibitGate.hpp:53-68` are
camelCase Qt-signal style, inconsistent with neighbouring `Transceiver.hpp` signals
(`tx_inhibit_changed`, `tci_mod_active`, `resolution`).

**Fix:** rename the four `TxInhibitGate` signals to snake_case for local consistency.
Cheap, and removes a legitimate style nit from review.

#### ☐ U3. The commit history is not PR-shaped

Current history interleaves feature work, branding, docs, tooling, and fixes across 21
commits (`2ac1be5` … `eeae769`), several of which are fixes to earlier commits in the
same series. Upstream asks for *"one PR per logical change."*

**Fix:** build the PR branch fresh from `WSJTX/wsjtx` `master` and construct a clean
series — ideally 3–5 commits (core logic + tests; transceiver integration; settings and
UI; UDP status message), or a single squashed commit. Do **not** rebase this fork's
history; author the patch deliberately.

#### ☐ U5. The Windows `\\.\COM` change — redundant, not harmful *(claim retracted)*

**Status: the breakage claim is RETRACTED — disproved on Windows 2026-08-09.**

Two successive corrections, both recorded here so the reasoning trail is honest:

1. *First reading* — "WSJT-X has a COM10+ CAT bug; this hunk fixes it."
   **Wrong:** Hamlib's `win32_serial_open()` already applies the prefix idempotently.
2. *Second reading* — "the hunk breaks CAT on every COM port via the unguarded
   `check_com_port_in_use()`." **Also wrong:** Windows normalizes `\\.\` paths, so the
   doubled prefix canonicalises back to the same device. Measured, not argued —
   see U6 and `docs/verification/u5-u6-windows-2026-08-09/`.

**Where that leaves the hunk: harmless and redundant.** It does not fix a bug (Hamlib
already handles COM10+) and it does not cause one (the double prefix normalizes away).
It is a no-op.

**What was in the working tree (2026-08-09), saved to
`scratchpad/wsjtx-com-path.patch`:** two hunks in `Transceiver/HamlibTransceiver.cpp` —
(1) add the `\\.\` extended-path prefix to the **CAT** port (`rig_pathname`), and
(2) make the existing **PTT** prefix conditional on `startsWith("COM")`.

**Why the premise fails.** Hamlib already applies the prefix, idempotently, for every
serial open — CAT and PTT alike. `win32_serial_open()`, `lib/termios.c:1366-1375`:

```c
/* according to http://support.microsoft.com/kb/115831
 * this is necessary for COM ports larger than COM9 */
if (memcmp(filename, "\\\\.\\", 4) != 0)
    SNPRINTF(fullfilename, ..., "\\\\.\\%s", filename);
else
    strncpy(fullfilename, filename, ...);
```

So passing plain `COM10` from WSJT-X works today. The CAT/PTT asymmetry in
`HamlibTransceiver.cpp` is real but harmless: the upstream PTT prefix is redundant,
not load-bearing.

**Why hunk (1) is actively harmful.** `serial_open()` — the CAT path — calls
`check_com_port_in_use(rp->pathname)` *before* opening (`src/serial.c:226`), and that
helper prepends **unconditionally**, with no idempotency guard (`src/serial.c:175`).
Feed it `\\.\COM3` and it builds `\\.\\\.\COM3`; `CreateFileA` fails; `serial_open`
returns `SER_NO_EXIST` → `-RIG_EIO` before ever reaching the idempotent open.

That breaks **every** COM port on Windows, not just COM10+ — the rig fails to open
with "serial port does not exist."

**Affected Hamlib releases:** `4.6.1, 4.6.2, 4.6.3, 4.6.4, 4.6.5, 4.7.0, 4.7.1, 4.7.2`
(`git tag --contains 028e8f38c`, added 2024-12-28). CI and `Build-Inhibit.ps1` pin
**4.7.1**, so this would have failed on first launch in the VM.

**Hunk (2) is a no-op.** `params.ptt_port` comes from `QSerialPortInfo::portName()`,
which yields bare `COM3` (`Configuration.cpp:5801` parses the number from index 3), so
the guard always takes the branch upstream already took. It only diverges for
non-`COM*` port names, where it silently stops prefixing — a small behaviour
regression with no upside.

**Action — now a judgement call, not a blocker.** The recommendation to drop it
stands, but on much weaker grounds: it is a no-op change to an upstream-owned file, so
it costs a rebase conflict site (§H.1) and one more hunk to strip at merge-back, for
no behavioural benefit. If you prefer belt-and-braces against some future Hamlib that
stops prefixing, keeping it is defensible — just mark it `FORK-ONLY` (§H.3).

The change is stashed (`git stash list` → "U5 com-path change (do not ship)" — the
stash message is now misleading) and also saved as
`docs/patches/wsjtx-com-path-DO-NOT-APPLY.patch`. Both should be renamed or dropped
once you decide; the "DO-NOT-APPLY" filename no longer reflects reality.

**Still unexplained:** if a real COM-port symptom originally motivated this change, it
has not been accounted for — Hamlib handles COM10+ and the prefix is inert. Worth
recalling what the original symptom was, because the actual cause is still unknown.

#### ⊘ U6. Hamlib `check_com_port_in_use()` is not idempotent — cosmetic, not a bug

**Not a WSJT-X change.** Within the same library, `win32_serial_open()`
(`lib/termios.c:1368`) guards the `\\.\` prefix and `check_com_port_in_use()`
(`src/serial.c:175`) does not. Any caller passing an already-extended path is rejected
before the open is attempted.

**Measured outcome: no functional impact.** See the "Why the double prefix is
harmless" subsection below — Windows normalizes `\\.\` paths, so the doubled string
resolves to the same device. What follows was the original (incorrect) reasoning,
retained for the trail.

**WSJT-X is such a caller today**: upstream sets `ptt_pathname` to `"\\\\.\\" + ptt_port`
unconditionally on WIN32 (`HamlibTransceiver.cpp:572-575`). The PTT port is opened via
`ser_open()`, which calls `OPEN` directly and *does* hit the idempotent path — so PTT
is safe. Only `serial_open()` (the CAT port) runs the unguarded check. **This is why
stock WSJT-X works and hunk (1) above does not.**

So the bug is currently latent for WSJT-X, but live for any Hamlib caller that passes
an extended path as `rig_pathname`.

**Proposed fix** — `scratchpad/hamlib-check-com-port-idempotent.patch`: reuse the same
guard as `win32_serial_open()`. Uses `strncmp` rather than the sibling's `memcmp`,
which reads a fixed 4 bytes and would run past the terminator on a port name shorter
than 4 characters.

**Status: VERIFIED (negative on hard-fail) — 2026-08-09 Windows.**

Full report: [`docs/verification/u5-u6-windows-2026-08-09/REPORT.md`](verification/u5-u6-windows-2026-08-09/REPORT.md).

| Check | Result |
|---|---|
| `rigctl -V` | Hamlib **4.7.1** `2026-04-15T20:20:01Z` SHA=`d042479` (WSJT-X bundled) |
| bare `COM1` via `serial_open` | **OK** |
| `\\.\COM1` via `serial_open` | **OK** — expected fail did **not** happen |
| CreateFile probe of unguarded double-prefix `\\.\\\.\COM1` | **OK** on Win 10.0.26200 |
| Dummy model `-m 1` | Does **not** open serial; useless for this test (use a serial model e.g. IC-7300) |
| U5 DO-NOT-APPLY rebuild | Not run; open already succeeds with extended path here |

**Revised takeaway:** the non-idempotent prefix is real in Hamlib source, but it is
**not** a functional bug. Do **not** treat U6 as an rc2 blocker.

### Why the double prefix is harmless (the mechanism the original analysis missed)

`\\.\` is the Win32 **device namespace** prefix, and — unlike `\\?\` — paths using it
are still **normalized** before the object manager resolves them. Normalization
collapses duplicate separators and drops `.` components:

```text
\\.\\\.\COM1     device prefix "\\.\"  +  remainder "\\.\COM1"
                 -> collapse duplicate "\\", drop the "." component
                 -> \??\COM1          (identical to the single-prefix result)
```

`\\?\` is the only prefix that bypasses normalization. Hamlib uses `\\.\`, so the
doubled path canonicalises back to the correct device. That is exactly what the probe
shows.

**The control row is what makes this conclusive.** `COM99` (a port that does not
exist) fails with `err=2` under *both* the single and double prefix. So the failure
mode observed is "no such device", never "malformed path" — the prefix count simply
does not affect the outcome in either direction.

**This generalises beyond the tested host — no USB re-test needed.** Normalization
happens in `ntdll` when the Win32 path is converted to an NT path, *before* the object
manager resolves the device name. The serial driver is handed a device object and
never sees the string at all, so FTDI / USB-CDC / motherboard UART cannot differ on
this point. The report's caveat about re-testing with a USB CAT cable is
over-cautious; the negative result is structural, not host-specific.

**Strategic reversal.** This was previously recommended as an easy first PR to
`Hamlib/Hamlib` to establish credibility before proposing TX Inhibit. **Withdraw
that.** A patch that fixes no observable behaviour, submitted as a bug fix, spends
maintainer attention and buys nothing. If sent at all it should be framed honestly as
a defensive-consistency cleanup between two functions in the same file — low value,
easily declined. Better to find a real first contribution.

**If a hard fail ever reproduces on another machine:** still goes to
`Hamlib/Hamlib`, not `WSJTX/wsjtx`.

---

### U5/U6 verification runbook (self-contained — for whoever has the Windows box)

Everything needed is in the repo; nothing depends on a scratch directory.

| Artefact | Path |
|---|---|
| The change that must NOT ship (for reproducing the breakage only) | `docs/patches/wsjtx-com-path-DO-NOT-APPLY.patch` |
| The proposed Hamlib fix | `docs/patches/hamlib-check-com-port-idempotent.patch` |

**Step 0 — confirm the Hamlib version is in range.** The bug exists in 4.6.1–4.7.2.

```bat
rigctl -V
```

If it reports something outside that range, none of the below applies; record the
version and stop.

**Step 1 — baseline: stock behaviour must work.** Build `main` *without* the
DO-NOT-APPLY patch. Configure a rig on a serial CAT port, and separately a
**different** COM port for PTT (`PTT method` = RTS or DTR, `PTT port` ≠ CAT port).

*Expected:* rig opens normally, PTT works. This proves `ser_open()` is idempotent-safe
and that upstream's unconditional `\\.\` prefix on `ptt_pathname` is harmless.

*If it fails instead:* U6 is worse than described — it would mean the PTT path is
affected too. Capture the error text and stop.

**Step 2 — reproduce the U5 breakage (the direct proof).**

```bat
git apply docs\patches\wsjtx-com-path-DO-NOT-APPLY.patch
```

Rebuild, then try to open **any** serial CAT rig — `COM3` is fine, it does not need to
be COM10+.

*Expected (as of 2026-08-09: **this did not happen** — kept only if you want to
re-confirm on different hardware):* rig fails to open, Hamlib log shows

```
serial_open: serial port \\.\COM3 does not exist
```

Note the logged path is what WSJT-X passed (`rp->pathname`); the doubled
`\\.\\\.\COM3` that actually fails `CreateFileA` is built inside
`check_com_port_in_use()` and never printed. Do not expect to see it in the log.

(or an `-RIG_EIO` / "cannot open" variant). **This is the whole finding.** If the rig
opens normally instead, U5 is wrong and the change is harmless — say so, because the
entire analysis was reasoned from source and never executed.

Then revert: `git checkout -- Transceiver/HamlibTransceiver.cpp`

**Step 3 — U6 independent of WSJT-X.** *(Run 2026-08-09: both invocations
succeeded. The steps below remain valid as a procedure.)* Test the Hamlib path
handling without WSJT-X at all.
**Do not use Dummy (`-m 1`)** — it never calls `serial_open`. Use any serial model
(e.g. IC-7300 `-m 3073`). High verbosity shows the open line:

```bat
rigctl -m 3073 -r "\\.\COM1" -vvvvv f
rigctl -m 3073 -r COM1 -vvvvv f
```

Look for `serial_open: serial port … is OK` vs `does not exist`.

*Original expected:* the first fails ("does not exist"), the second works.

*Observed 2026-08-09 (COM1, Win 10.0.26200, Hamlib 4.7.1):* **both OK**. See
`docs/verification/u5-u6-windows-2026-08-09/`.

**Step 4 — confirm the fix.** Rebuild Hamlib with
`docs/patches/hamlib-check-com-port-idempotent.patch` applied, then repeat step 3.

*Expected:* both `rigctl` invocations now work.

**What to report back:** the `rigctl -V` version, whether each step matched
"Expected", and the exact error text where it did not. A negative result is just as
useful — it retires U5/U6 and means the working-tree change was harmless after all.

**Strategic note (still holds, redirected).** `CONTRIBUTING.md:131` requires one PR per
logical change, so a small, self-contained fix like this is a good first contribution —
it establishes you as someone who sends clean patches before the much larger TX Inhibit
proposal arrives. The target project changed; the reasoning did not.

#### ☐ U4. Baseline is v3.0.2; upstream `master` has moved

`UPSTREAM.md` pins `ccdfaf3` / v3.0.2. The PR must apply to current `master`, and the
public repo only updates at release time — so `master` may be *behind* what the team is
actually working on internally. Expect merge friction in `Configuration.cpp` (a 236 KB
file that changes often) and in `mainwindow.cpp`.

**Fix:** rebase onto current `WSJTX/wsjtx` `master` immediately before opening the PR,
and rebuild + rerun tests on that base.

---

### G.5 Track A plan

**Phase A1 — decide the design questions.** No code. Settle Q1 (transport), the
RTS/DTR-vs-all-PTT question (G.2), Q3 (failure policy), and Q4 (security posture).
These change what gets written; deciding them after writing is how 900 lines get
written twice.

**Phase A2 — open the upstream Issue.** Problem statement, proposed approach, the Q1
rationale, and an offer to submit a PR. Gather reaction. Cite the issue number later.

**Phase A3 — fix the code findings that a reviewer would reject on.**
In priority order: **C2** (exceptions escaping into the event loop → `qFatal`),
**C3** (silent inhibit under four rig types), **C1** (wall-clock time base),
**C5** (unconditional `ptt_on_` — an unrelated change to a stock path, which by
`CONTRIBUTING.md`'s "one logical change" rule alone must not be in this PR),
**C4** (fail-silent), **C6**, **U1**, **U2**.

C2, C3, C5, and U1 are the four a reviewer is most likely to catch and be annoyed by.

**Phase A4 — trim.** Strip everything in the §G.1 "Out" list. Trim
`docs/TX_INHIBIT.md` per G.2. Resolve D3 (rename `gate`/`block`/`intent` identifiers,
then delete the self-critical note at `docs/TX_INHIBIT.md:503-504` — do not ship a spec
that tells the reviewer the code doesn't match it).

**Phase A5 — rebuild the branch.** Fresh clone of `WSJTX/wsjtx` per `CONTRIBUTING.md`,
current `master`, clean commit series (U3, U4). Build and run the full test suite on
that base, on as many of Linux/Windows/macOS as you can reach.

**Phase A6 — write the PR description.** What, why, platforms tested, issue number.
Q1–Q5 answered pre-emptively. Include the two JSON payloads and a one-liner so a
reviewer can exercise it in two minutes without building a KEY agent.

**Dependency:** A3 and A4 can proceed in parallel with A2's discussion. A5 must be last.
Track B Phase 1 (unblock CI) should happen early regardless, because it is how you get
the platform evidence Phase A6 needs.

---

### G.7 (see also) §H — fork maintenance

The merge-back shape described above is the same shape that makes rebases cheap.
§H covers the ongoing discipline.

---

### G.6 Honest assessment of reception

**Working in the feature's favour:** the problem is real and well-known (multi-op /
same-band / SO2R interlock); nothing in WSJT-X addresses it today; `Halt Tx` is a poor
substitute and everyone who has tried it knows so; the implementation is small, opt-in,
default-off, and unit-tested; and the design document is more rigorous than most.

**Working against it:** it adds a *second* network protocol on a *second* port to an
application that already has one and whose maintainers have spent years on its
backward-compatibility discipline (see the rules quoted in D1). Q1 is not a formality —
it is the decision the PR lives or dies on. It also asks a seven-person volunteer team
to take on permanent maintenance of a feature that serves a minority of users and
requires external software (a KEY agent) that upstream would not control or ship.

**The most likely outcomes, roughly ordered:** (a) interest, followed by a request to
rework the transport onto the existing protocol; (b) interest, followed by a long quiet
period; (c) acceptance largely as-is if Q1 is argued convincingly and the code is clean.

**What most improves the odds, in order:** answer Q1 before writing anything; cut the
diff to the §G.1 list; fix C2/C3/C5/U1; and arrive with real multi-op operating reports
from Track B. The last one is the hardest to argue with and the only one that cannot be
produced quickly — which is the real reason Track B matters.

---

## H. Track C — maintaining the fork across rebases

Added 2026-08-09 when the goal was restated as *"a maintained parallel fork that
rebases as needed, eventually merged back."* This section is about the recurring cost
of that, which none of §A–§G addressed.

### H.1 The core economics

Rebase cost is roughly proportional to **how many upstream-owned files you touch**,
weighted by **how often upstream changes them**. New files under `TxInhibit/` cost
nothing to rebase — they never conflict. Every hunk in `Configuration.cpp` costs
something on every rebase, forever.

Current footprint in upstream files, worst first:

| File | Size / churn | What the fork changes | Verdict |
|---|---|---|---|
| `Configuration.cpp` | 236 KB, changes constantly | ~8 small hunks (setting, signals, gate wiring) | **Unavoidable** — this is where the setting belongs |
| `widgets/mainwindow.cpp` | large, high churn | badge + `InhibitStatus` (~45 lines) | Unavoidable, but see H.3 |
| `CMakeLists.txt` | high churn | source list, `inhibit-test` target, install, CPack identity | Mixed — some avoidable |
| `Transceiver/HamlibTransceiver.cpp` | moderate | the pin filter — the actual feature | Unavoidable, and correct |
| `revision_utils.cpp` | low churn | branding only | **Pure tax** — see H.2 |
| `widgets/about.cpp` / `about.ui` | low churn | branding only | **Pure tax** |
| `widgets/mainwindow.ui` | very high churn | 2 menu strings | **Pure tax on a high-churn file** |
| `Configuration.ui` | very high churn | 1 checkbox | Unavoidable |
| `CMakeLists_wsjtx.txt` | n/a | not an upstream file at all | Delete or ignore |

### H.2 Branding is the cheapest thing to give up

☐ **H2.** The identity changes (`revision_utils.cpp`, `about.cpp`, `about.ui`,
`mainwindow.ui` menu text, the `Ctrl+F1` shortcut row in `mainwindow.cpp`) deliver one
thing: a tester can tell which build they are running. They cost a rebase conflict
every time upstream touches those files — and `mainwindow.ui` is one of the
highest-churn files in the tree.

Cheaper ways to get the same benefit with a near-zero rebase surface:

- The **package identity** work just completed (R3/R5) already distinguishes the
  install directory, Add/Remove entry, and filenames — arguably the places that
  actually matter for "which build is this?"
- The **status bar** already has a fork-specific badge. Extending it to show the
  listening state (C4) doubles as a build marker.
- If an About-box marker is wanted, appending one line is far less conflict-prone
  than restructuring the whole HTML block, which is what `about.cpp` does today.

Also note the branding is what breaks `test_http_user_agent` (R2). Reverting
`http_user_agent()` closes R2, removes a rebase conflict, and loses nothing —
it is only read by `Network/Cloudlog.cpp`.

### H.3 Conventions that make rebase and merge-back mechanical

☐ **H3.** Adopt and apply consistently:

1. **Mark every fork-only hunk.** Started in this pass:
   ```
   # ---- FORK-ONLY (wsjtx-inhibit): drop this hunk when rebasing for an upstream PR ----
   ...
   # ---- end FORK-ONLY ----
   ```
   Applied so far to the CPack identity block and the Debian relationship lists in
   `CMakeLists.txt`, and the two `CMakeCPackOptions.cmake.in` hunks. Extend to the
   remaining branding hunks. A `grep -rn "FORK-ONLY"` then *is* the strip list for the
   merge-back PR — no judgement calls at PR time.
2. **Prefer new files over edits to existing ones.** Already true for `TxInhibit/`.
3. **Keep hunks small and adjacent.** Several small edits scattered through a function
   conflict more than one block at a known location.
4. **Never reformat upstream code.** A whitespace change in a large file turns a clean
   rebase into a manual merge.

### H.4 Rebase vs merge — decide this before the first rebase

☑ **H4. DECIDED 2026-08-09 — two branches.** *"Rebases as needed"* on a **published** fork rewrites history. Anyone who
cloned `wa1hco/wsjtx-inhibit` — testers building from source, and any future
contributor — gets a non-fast-forward on their next pull.

Options:

| Approach | Effect | Cost |
|---|---|---|
| **Rebase + force-push `main`** | Clean linear history; the merge-back diff stays minimal and readable | Breaks every existing clone on every rebase |
| **Merge upstream into `main`** | Nobody's clone breaks | History accumulates merge commits; the eventual merge-back diff is messier to extract |
| **Hybrid (suggested)** | `main` only ever merges (safe for testers); a separate `upstream-pr` branch is rebased and force-pushed freely | Two branches to keep in step |

The hybrid matches the two audiences: testers need a stable branch, the merge-back
needs a clean one.

### H4.1 The decided policy

| Branch | Audience | Upstream integration | History | Tags |
|---|---|---|---|---|
| **`main`** | testers, package builds | **merge** from upstream | append-only, **never force-pushed** | `build/v*`, `packages/v*` |
| **`upstream-pr`** | the merge-back proposal | **rebase** onto current upstream `master` | rewritten freely, force-pushed | none |

**Rules that make this work:**

1. **All development lands on `main` first.** It is the branch that gets built,
   packaged, and tested. `upstream-pr` is a derived artefact, not a place to work.
2. **`upstream-pr` is disposable.** Rebuild it from scratch when preparing a proposal
   rather than maintaining it continuously — cherry-pick or re-author the feature
   commits onto current `master`, strip everything marked `FORK-ONLY` (§H.3), and
   force-push. Trying to keep it continuously in sync doubles the rebase work for no
   benefit, since upstream only sees it once.
3. **Never merge `upstream-pr` back into `main`.** One-way flow only. Merging a
   rebased branch into a merge-based branch produces duplicated commits.
4. **Testers are never asked to force-pull.** That is the whole point of the split, and
   it should be stated in `UPSTREAM.md` (§H5) so it is a promise, not an accident.

**Consequence for §H.2 (branding) and U5:** both become cheaper. Anything marked
`FORK-ONLY` is stripped mechanically when `upstream-pr` is built, so carrying fork-only
changes on `main` costs only rebase-conflict noise during upstream merges — not
merge-back friction. That weakens (but does not remove) the argument for shedding the
branding: the remaining cost is conflicts in `mainwindow.ui` and friends on every
upstream merge.

**Still to do:** document the policy in `UPSTREAM.md` (§H5), and create
`upstream-pr` only when a proposal is actually being prepared — not before.

### H.5 `UPSTREAM.md` should become a living rebase record

☐ **H5.** Today it is a one-time pin (baseline `ccdfaf3`, v3.0.2). For a fork that
rebases repeatedly it should carry:

- Current upstream base commit **and date**, updated at each rebase.
- A short rebase log — what conflicted, and how it was resolved. Conflicts recur in
  the same places; notes turn a 2-hour rebase into a 20-minute one.
- The branch policy from H4.
- The rebase runbook: rebase → build → `ctest` → smoke-test TX Inhibit → re-tag.

### H.6 The test suite is the rebase safety net, and it is thin

☐ **H6.** `test_tx_inhibit_logic` (12 cases) covers the **pure** logic only —
parsing, hold timeout, badge text, counters. Nothing covers the part a rebase is most
likely to break: the integration in `HamlibTransceiver::do_ptt`, the gate lifecycle
across `do_start`/`do_stop`, and the signal path out to `Configuration`.

That is exactly the code most exposed to upstream churn, because `HamlibTransceiver`
and `Configuration` are upstream-owned. A rebase that silently breaks the pin filter
would not be caught by any current test — only by a human with a radio.

**Suggested minimum before the first rebase:** a test that drives `GateLogic` through
the state machine a `do_ptt` caller would (intent on → hold → intent still on →
release → assert), plus a fake-transceiver test for gate start/stop lifecycle. Neither
needs hardware.

This compounds with **C1** (monotonic clock): making time injectable for testing and
making it robust against clock steps are the same change.

### H.7 Ordering

1. **H3** conventions (cheap, immediately useful, makes everything after easier)
2. **H4** branch policy decision — needed *before* the first rebase, not after
3. **R2 + H2** drop the branding that costs the most and returns the least
4. **H6** integration tests, then **C1–C5**
5. **H5** `UPSTREAM.md` runbook, written *during* the first rebase while it is fresh
