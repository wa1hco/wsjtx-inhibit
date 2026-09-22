# Draft update — DG2YCB + Improved list (3.2.0 upgrade)

Status: **draft — review before send**  
Style: lite Simplified Technical English  
Baseline: WSJT-X Improved **3.2.0 PLUS_260908**  
Patch: `contrib/improved-list-submission/tx-inhibit-upgrade-3.2.0_improved_PLUS_260908.patch`  
Apply notes: `contrib/improved-list-submission/README-3.2.0-PLUS-260908.md`  
Reference binaries (mainline fork): https://github.com/wa1hco/wsjtx-inhibit/releases/tag/build/v3.0.2-rc5  

Suggested recipients:

- Uwe DG2YCB (direct)
- WSJT-X Improved community list: `wsjt-x-improved-community@lists.sourceforge.net`
- wsjtx-inhibit tester group (same body; adjust greeting)

Attach the `.patch` file. Do not paste it inline.

Plain-text email body (ready to paste): `docs/announce-rc5-email.txt`

Copy everything below the horizontal rule when ready (markdown draft).

---

**Subject:** TX Inhibit upgrade for Improved 3.2.0 PLUS_260908 (type 18 + port-0 fix)

---

Hello Uwe and all,

Improved 3.2.0 PLUS_260908 already includes TX Inhibit from the earlier
submission. That design used JSON holds and preferred UDP port 22372.

Review comments from the WSJT-X group asked for existing 
message formats instead of JSON. This note offers an 
**upgrade patch** against the 3.2.0 Improved drop.
It replaces the in-tree JSON/22372 path with the current design.

## What the upgrade does

Terms in this note:

- **UDP inhibit endpoint** = `host:<udp-port>` for type-18 holds.
- **PTT serial device** = Settings → Radio → Port for RTS/DTR.

| Area | In Improved 3.2.0 today | After this upgrade |
|---|---|---|
| Hold wire | JSON on preferred UDP **22372** | `NetworkMessage::TxInhibit` **type 18** |
| Listen port | Prefer 22372; may fall back | **Always ephemeral**; type **17** announces it |
| Multi-controller | One global hold | **Per-controller leases**, OR’d |
| KEY agent | Bench helpers | **`inhibit-agent` / `inhibit-agent-gui`** (CTS → type 18) |
| Port 0 | Could be announced | Live type 17 never uses port 0 |

Wire authority: https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md

Point KEY agents at the ephemeral `host:port` from InhibitStatus announcement, message 17. Confirm the port is non-zero after the rig opens.

## Contest bug fixed in this upgrade

At W2SZ in September, InhibitStatus (type 17) announced port 0 and was debugged as a race conditions in Qt 5.
Inhibit controllers reject port 0.
The KEY-agent target list stayed empty.
No holds reached the WSJT-X instances.

Qt 5 can return true from `QUdpSocket::bind(0)` while `localPort()` is still 0.  A second path also published port 0 on the 15 s type-17 pulse while Hamlib `rig_open` was still running.

**Fix in this patch:**

- After bind, if `localPort()` is 0, read the port with `getsockname`.
- If the port is still 0, treat bind as failed.
- Do not emit `portBound(0)`.
- Send a live type 17 only with a non-zero port.
- Port 0 remains the disable/clear announce only.
- Windows links `ws2_32` so `getsockname` and `ntohs` resolve.

## Apply

```text
tar xzf wsjtx.tgz          # inner tarball from 3.2.0 PLUS_260908
cd wsjtx
patch -p1 --binary < tx-inhibit-upgrade-3.2.0_improved_PLUS_260908.patch
```

`--binary` is required. Improved sources are CRLF. The patch is CRLF too.

Do not apply the older 3.1.0 add-on patch on this 3.2.0 tree.
That tree already has Inhibit.

SHA-256:
`9aa4e740cfff5fddb4af05f97d047f62fcddb4b67af4b142e1bd7e09b13fcff0`

## Reference build

The same design runs on the mainline-based fork:
https://github.com/wa1hco/wsjtx-inhibit/releases/tag/build/v3.0.2-rc5

Assets there: Windows installer; Linux AppImage, `.deb`, and `.rpm`.

I can adjust structure or wording to match how you want this carried
in Improved.

73,
Jeff, WA1HCO
