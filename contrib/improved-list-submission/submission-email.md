# Draft: TX Inhibit submission to WSJT-X Improved

WSJT-X Improved has **no source repository** — the SourceForge project carries only
Files, Discussion, Support, Reviews, and Mailing Lists. So there is no pull request to
open; a contribution is a patch plus a description, sent to the maintainer.

Project developers are listed as **dg2ycb** (Uwe Risse, the publisher) and **w3sz**
(Roger Rehr). W3SZ is a VHF/microwave and EME operator, so the multi-op
single-transmitter problem this solves should need no explaining.

**Suggested route:** post to the community list, which is public and archived, and
copy Uwe directly. The list gives other Improved users a chance to say whether they
want this; a direct mail alone can stall silently.

- List: <wsjt-x-improved-community@lists.sourceforge.net>
  (subscribe first: <https://sourceforge.net/projects/wsjt-x-improved/lists/wsjt-x-improved-community>)
- Uwe DG2YCB, via <https://www.qrz.com/db/DG2YCB>

**Attach** `tx-inhibit-3.1.0_improved_AL_PLUS_260522.patch`. Do not paste it inline —
it is CRLF by design and mail clients mangle line endings, which is precisely what
breaks the apply. Mention the GitHub link as the browsable alternative.

---

Subject: TX Inhibit for multi-op single-transmitter interlock — patch against 3.1.0 improved AL_PLUS 260522

Hello Uwe, Roger, and all,

I'd like to offer an opt-in TX Inhibit function for WSJT-X Improved, and
ask whether you'd consider it for the mainline Improved builds.

The problem: multi-op stations sharing a band — in our case the W2SZ VHF
contest group, interleaving FT8 with SSB/CW — must keep only one
transmitter keyed at a time. Halt Tx is the wrong instrument, because it
aborts the QSO sequence. What an interlock needs is to hold off the
radio's PTT line within milliseconds while sequencing and audio continue
untouched, then release PTT the moment the band is free.

How it works: a small gate sits on the RTS/DTR PTT path and computes
"assert PTT if and only if (want_tx and not hold)". Holds arrive as
compact JSON datagrams on UDP 22372, each carrying a TTL, so a lost
release packet fails safe — the hold expires on its own. A KEY agent
watching the priority radio's key line sends the holds. There is
deliberately no remote control of TX, only inhibition of it. While a
hold is active the status bar shows a red TX INHIBITED badge.

It is off by default. With the box unticked the gate is never
constructed, no socket is bound, and PTT behaves exactly as it does
today. It applies to RTS/DTR PTT only; CAT PTT is untouched.

Included are two QtTest suites — one over the pure gate logic, one over
a real UDP socket and event loop — plus a bench sender so the feature
can be exercised without interlock hardware. Applying the patch to a
freshly extracted src/wsjtx.tgz, everything builds and ctest passes 3/3.
It has been tested on the air here: inhibit asserted mid-transmission
during a live QSO, PTT dropped, and sequencing carried on as intended.

One note on the patch: the Improved drops are CRLF throughout, so the
patch is CRLF too and needs

    patch -p1 --binary < tx-inhibit-3.1.0_improved_AL_PLUS_260522.patch

Without --binary, GNU patch rejects every hunk on "different line
endings".

The patch also restores the enable_testing() call that mainline WSJT-X
makes before add_subdirectory (tests). It is unrelated to this feature,
but without it every add_test() is a silent no-op and ctest reports no
tests at all — you may want that change regardless of what you decide
about the rest.

The same feature on official mainline is at
https://github.com/wa1hco/wsjtx-inhibit, and the Improved port,
browsable by file, is at https://github.com/wa1hco/wsjtx-improved-inhibit.
Both are GPL-3.0, same as the host project.

I'm glad to rework any of it — the protocol, the UI wording, where the
setting lives, or the structure — to fit how you'd want it carried in
Improved. And if you'd rather not carry it at all, that's entirely fine;
I'll keep maintaining the port against your drops either way.

73,
Jeff, WA1HCO
