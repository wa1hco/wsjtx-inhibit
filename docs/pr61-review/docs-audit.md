# Docs consistency audit — TX Inhibit wire / leases / ephemeral listen

Date: 2026-09-07 (updated)  
Skill: `.grok/skills/docs-consistency`  
Authority: `docs/TX_INHIBIT.md` (type **18** + per-controller leases + **ephemeral** listen + type **17** announce)  
Mode: live docs aligned this pass; archives bannered.

## Summary

| Kind | Count | Action |
|------|-------|--------|
| **live — fixed this pass** | BUILDING, INSTALL*, upstream-pr*, INHIBIT_AGENT, TX_INHIBIT cadence, tester README, Configuration Enable-Inhibit tooltip | Match current code |
| **archive — banner** | REVIEW-rc2, release-notes-rc2, verification/rc2-windows-checklist | Superseded banner; bodies left |
| **negotiation** | `docs/pr61-review/*` drafts | Leave; grok-review updated with resolutions |
| **OK** | TX_INHIBIT authority (aside from cadence wording now fixed) | |

## Current published facts (must match live docs)

- Type **18** hold; no JSON wire  
- Per-controller leases, OR’d  
- Ephemeral inhibit listen port; learn via type **17** (bind/clear + hold/badge + 15 s pulse)  
- Empty Id = any; non-empty must match instance Id  
- `inhibit-agent-gui`: **NEED GATE** until Apply  
- Enable TX Inhibit requires RTS/DTR + non-empty Port  

## Still open (code / policy, not docs)

- Standalone type-17 discovery in `inhibit-agent`  
- Gate integration tests for Id / multi-controller / announce  
- `inhibit-sim` upstream packaging policy  
- Port-list refresh / preserve-custom-path UX on branch `settings-serial-port-ux`  

## Archive banners applied

| file | banner |
|------|--------|
| `docs/REVIEW-rc2.md` | Historical; see TX_INHIBIT.md |
| `docs/release-notes-wsjtx-inhibit-rc2.md` | Historical RC2 |
| `docs/verification/rc2-windows-checklist.md` | Historical; do not use JSON/22372 steps on current builds |
