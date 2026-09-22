# TX Inhibit upgrade for Improved 3.2.0 PLUS_260908

This patch **upgrades** the in-tree TX Inhibit that already ships in
WSJT-X Improved **3.2.0 PLUS_260908**.

It does **not** add Inhibit from scratch. The old design used preferred
UDP **22372** and JSON-style holds. This upgrade matches the current
wsjtx-inhibit design.

## Baseline

| Item | Value |
|------|--------|
| Against | WSJT-X Improved 3.2.0 `PLUS_260908` (`src/wsjtx.tgz` inner tree) |
| Patch | `tx-inhibit-upgrade-3.2.0_improved_PLUS_260908.patch` |
| SHA-256 | `9aa4e740cfff5fddb4af05f97d047f62fcddb4b67af4b142e1bd7e09b13fcff0` |
| Endings | CRLF on purpose (same as the 3.1.0 archive patch) |

## What changes

- Always-ephemeral UDP listen port (no preferred 22372 bind).
- NetworkMessage type **18** `TxInhibit` holds (per-controller leases).
- Type **17** `InhibitStatus` announce stays; live announces never use port 0.
- Pulse-period type-17 announce timer in MainWindow.
- `getsockname` recovery when Qt `localPort()` stays 0 after `bind(0)`.
- Windows links `ws2_32` for that path.
- `set_instance_id` for type-18 Id matching.
- `inhibit-sim` PTT port helper.
- Settings checkbox label: Enable / enabled / failed (no port).
- `tools/inhibit-agent` plus updated `inhibit-test` / helper scripts.
- Docs under `docs/TX_INHIBIT.md` and `docs/INHIBIT_AGENT.md`.
- Test hook: `WSJTX_TX_INHIBIT_FORCE_BIND_FAIL=1` forces arming failure.

Improved branding in CMake stays unchanged.

## Apply

```text
tar xzf wsjtx.tgz
cd wsjtx
patch -p1 --binary < tx-inhibit-upgrade-3.2.0_improved_PLUS_260908.patch
```

`--binary` is required. Without it, GNU patch fails hunks on line endings.

## Related archive

The older add-on patch for Improved 3.1.0 lives beside this file:

- `tx-inhibit-3.1.0_improved_AL_PLUS_260522.patch`
- `README.md` (archive notes for that submission)

Do not apply the 3.1.0 patch on a 3.2.0 tree that already has Inhibit.

## Known leftover

`tools/inhibit_spacebar/` (Improved Windows stand-in) still mentions port
22372. This upgrade does not rewrite that helper. Prefer `inhibit-test`
or `inhibit-agent` with the ephemeral port from type 17.
