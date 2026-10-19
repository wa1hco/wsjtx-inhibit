# WIMS docs cleanup — all WSJT-X on UDP 2237

Saved from a Grok chat (2026-09-02) so it is findable later.
Repo: https://github.com/wa1hco/WIMS
Product: WSJT-X Instance Management System (WA1HCO), not educational WIMS.

## Decision (current, authoritative)

Plane A:

- UDP Server: `224.0.0.73`
- UDP port: **2237 for every WSJT-X instance**
- Accept UDP requests: ON
- Outgoing interface: contest LAN NIC (still the #1 silent failure)
- Unique `--rig-name` per instance — this is how WIMS/N1MM tell radios apart, **not** the port
- WIMS server joins **2237** (solo and fleet default)
- Do **not** send operators to 2238–2243
- **2240** stays a “don’t use” footnote only (N1MM hole), not part of a band map

Old Scheme A (one UDP port per band: 50→2237, 144→2238, 222→2239, 432→2241, 902→2242, 1296→2243) is **retired**.
`wims_networking.md` used to call 2237-everywhere “Scheme B” and left Scheme A as the recommended default. That is what made the docs a mess.

## Still needs an explicit design sentence

Do not leave both schemes live. Write these in `docs/plan/wims_design.md` + `docs/plan/wims_networking.md` §4:

1. How does **one N1MM per band** filter a **shared 2237** stream? (rig-name list vs band from Status vs only one N1MM reader on the LAN)
2. Is the per-band port map fully retired, or a lab-only `--ports` escape?
3. GridTracker: join 2237 view-only; experimental `--gt-forward` stays lab-only

Until those sentences exist, every other file will drift again.

## Files that still teach the old map

| File | Stale claim | Change |
|------|-------------|--------|
| `docs/plan/wims_networking.md` §1, §3.3, §4.3–4.6 | 50→2237 … 144→2238 … fleet joins 2237–2243 | Replace §4.3 with “all WSJT-X = 2237”. Move Scheme A to **Historical / retired**. |
| `docs/plan/wims_status.md` “How to run” | band ports + `solo --port 2238` + server joins six ports | Solo/server default **2237 only** |
| `docs/tester_quickstart.md` B2 + Track D | band port table | One row: port **2237** |
| `docs/User-manual.md` Networking | “band ports 2237+ skip 2240” | “2237 all instances” |
| `scripts/windows/README.md` | “or your band port — 144→2238” | 2237 only |
| Agent/setup check strings | may flag a correct 2 m seat as wrong | Audit `src/wims/agent` copy |

README is almost clean (it defers to networking). The rot lives in plan + tester docs.

## Other old/new collisions (same pass, separate from 2237)

1. **How you start the program** — status says `python -m wims` (checkbox launcher) and `python -m wims solo`; README still leads with `python3 src/wims/server/app.py`. Pick one recommended path; list the others as aliases.
2. **Phase-1 “read-only” vs click-to-Work** — README status line still sounds read-only; R0 already has roster Work + Halt. Say “Phase-1 console live; R0 Work path in progress / dummy-load remaining.”
3. **Design vs status mixing** — networking.md still carries open Switchboard / F4 Option 1 / GT-bridge decisions in the header. Those belong in design or a short “Open decisions” box, not in the operator map.
4. **`FLEET_WSJT_PORTS`** — cited in networking.md; may not match a real code symbol. Docs must name the actual default in server/cli.
5. **Python version** — `CLAUDE.md` says “Python 3.14”; README/CI say 3.10 / 3.12 / 3.14. Write “3.10+; CI on 3.10/3.12/3.14.”
6. **KEY agent** — “not in R0” in tester roles vs launcher checkboxes that mention Key. One sentence: shipped in launcher, not a tester requirement.
7. **Pages** — CLAUDE.md still says two-phase UI; product is Operate / Status / Setup.

## Cleanup order (small commits)

1. **Decision commit** — ~20 lines at the top of `wims_networking.md` §4: “Current: all WSJT-X use 2237. Per-band ports retired.” Plus the N1MM-on-shared-stream rule.
2. **Operator docs** — tester_quickstart, User-manual, windows README, status “How to run.”
3. **Historical appendix** — old 2237–2243 table, labeled retired.
4. **Code/doc lint** — grep `2238|2241|band port|FLEET_WSJT_PORTS|Scheme A` and update agent messages or mark lab-only.
5. **Docs contract** in `CLAUDE.md`: operator docs must not contradict §4 current scheme.

Do not rewrite the 1800-line design doc in the same commit.

## Prompt for Grok Bot / Grok Build

```text
Repo: wa1hco/WIMS. Docs-only unless a string in src/wims/agent
hard-codes the old band-port table.

Current networking (authoritative, 2026-09):
- Every WSJT-X: UDP 224.0.0.73 port 2237
- Unique --rig-name per instance
- WIMS joins 2237 by default (solo and fleet)
- Per-band ports 2238/2239/2241–2243 are RETIRED
- 2240 remains unused (N1MM hole) but is not a “band”
- Outgoing interface = LAN is still required

Do:
1. Grep the tree for 2238, 2239, 2241, 2242, 2243, "band port",
   Scheme A, FLEET_WSJT_PORTS, solo --port 2238.
2. Patch operator docs first: tester_quickstart.md,
   User-manual.md, scripts/windows/README.md,
   docs/plan/wims_status.md "How to run".
3. In wims_networking.md, replace §4.3 default map with the
   2237-everywhere rule. Move the old table to "Retired Scheme A".
4. Do not invent how N1MM filters a shared 2237 stream — add a
   TODO if the design doesn't say.
5. No code behavior change except agent/setup help text that
   still tells the user to use 2238 for 2 m.
6. Do not commit.

Return a table: file, old sentence, new sentence.
```

## Suggested landing place in the repo

`docs/plan/wims_docs_debt.md` (or drop this file at repo root while the sweep runs).
Keep design changes in `wims_design.md` and landed-code notes in `wims_status.md`.
This note is **docs debt / cleanup checklist**, not design and not status.
