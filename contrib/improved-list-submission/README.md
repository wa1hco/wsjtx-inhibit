# Improved list submission artifacts

This directory holds patches and cover letters for WSJT-X Improved.
These are **not** patches against the mainline `wsjtx-inhibit` tree.
Do not apply them here.

## Current (use this for Improved 3.2.0)

| File | Role |
|------|------|
| `tx-inhibit-upgrade-3.2.0_improved_PLUS_260908.patch` | Upgrade in-tree JSON/22372 Inhibit to type 17/18 |
| `README-3.2.0-PLUS-260908.md` | Apply notes + SHA-256 |
| `submission-email-3.2.0-PLUS-260908.md` | Cover-letter pointer |
| `../../docs/announce-rc5-update.md` | Mail body draft |

## Archive: first submission (Improved 3.1.0)

This block is a **verbatim backup** of the first TX Inhibit submission to
the WSJT-X Improved community list.

| Item | Value |
|------|--------|
| List | `wsjt-x-improved-community@lists.sourceforge.net` |
| Against | WSJT-X Improved 3.1.0 `AL_PLUS_260522` (`src/wsjtx.tgz`) |
| Live copy | https://github.com/wa1hco/wsjtx-improved-inhibit/tree/main/contrib/tx-inhibit |
| Source commits | `360ccd9` (add) + `035414e` (store patch as `-text`) |
| Patch SHA-256 | `ff998a944168f5f2e25fdd4416a14cb9d233ee93d17589ca4b5f2392f4bdf427` |
| Size / endings | 188627 bytes, 4896 CRLF lines, 0 LF-only lines |

## Files

| File | What it is |
|------|------------|
| `tx-inhibit-3.1.0_improved_AL_PLUS_260522.patch` | The attachment. CRLF on purpose. |
| `submission-email.md` | Cover letter drafted for the list + DG2YCB. |

## Apply (Improved drop only)

```text
tar xzf wsjtx.tgz          # inner tarball from the 260522 superbuild
cd wsjtx
patch -p1 --binary < tx-inhibit-3.1.0_improved_AL_PLUS_260522.patch
```

`--binary` is required. Without it GNU patch fails every hunk on
"different line endings".

## What this snapshot is not

The later **260818 review seams** (`pttApplyFailed`, `close_rig` UI
clear, RTS/DTR-only enable, Windows `inhibit-test` `_kbhit`) live in
this repo at `15b5799b1`. They are **not** in the mailed patch and
were **not** committed to `wsjtx-improved-inhibit`.
