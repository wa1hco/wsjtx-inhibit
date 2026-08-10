# Draft announcement — W2SZ contest list, rc2

**Send only after the release page actually lists assets.** Pushing the tag starts a
multi-platform build; if it fails there is nothing to download and the links below go
to an empty page. Check
<https://github.com/wa1hco/wsjtx-inhibit/releases> first.

---

**Subject:** WSJT-X with TX Inhibit — rc2 available for testing (multi-op / same-band)

---

Gang,

I have a second release candidate of **wsjtx-inhibit** ready for anyone willing to
bang on it. It is WSJT-X 3.0.2 with one addition: a way to stop a digital station
keying while another op on the same band is transmitting.

**The problem it addresses.** In a multi-op setup with more than one radio on a band,
an unattended FT8 station will happily key right on top of the SSB or CW operator.
The usual fix is Halt Tx, which kills the QSO sequence and means restarting the
contact. TX Inhibit instead holds the **PTT line** off while leaving FT8 sequencing
and audio running. The digital station simply misses that transmission and carries on
with the next one. In practice that lets you interleave FT8 with voice and CW on one
band without babysitting the digital seat.

**How it works.** A "KEY agent" watches the priority radio's key line and sends a
small UDP message to each WSJT-X station telling it not to transmit. Holds are
refreshed a few times a second and expire on their own if the agent stops, so a dead
agent or an unplugged network cable fails back to normal transmit rather than a
permanently muted station.

**What you need**

- Windows 64-bit or Linux. macOS is not tested.
- **PTT method RTS or DTR on a real serial port.** This is the one hard requirement.
  CAT PTT is *not* filtered — if your PTT method is CAT, TX Inhibit does nothing.
  Sharing one USB COM port between CAT and RTS/DTR is fine and normal.
- Something to generate the holds. A KEY agent that senses your priority radio's key
  line is the real answer; a bench helper ships with the package so you can test
  without building anything.

**Download:** <https://github.com/wa1hco/wsjtx-inhibit/releases> — look for
`wsjtx-inhibit-3.0.2-rc2-win64.exe`, or the portable `.zip`, under Assets. Linux
AppImage / `.deb` / `.rpm` are there too.

Install instructions: `INSTALL-WINDOWS.md` in that repo.

**It installs beside your existing WSJT-X, not over it** — that was the main thing
fixed since rc1. Program files land in `C:\WSJT\wsjtx-inhibit`, with its own entry in
Add/Remove Programs. One caveat: it still *shares settings* with a normal WSJT-X
install. If you would rather it did not touch your working configuration, add
`--rig-name inhibit` to the shortcut and it will keep its own.

**What would help most**

1. Does it behave exactly like stock WSJT-X with TX Inhibit switched off? That is the
   default, and any difference is a bug I want to hear about immediately.
2. With it on and a hold applied, does your rig stay unkeyed while WSJT-X continues
   its cycle? Dummy load for the first try.
3. Anything odd about the install, especially alongside an existing WSJT-X.

**Being straight about the state of it.** The PTT path has been tested on the bench
and against a real radio, and the logic has an automated test suite. It has **not**
been run in an actual contest with multiple stations yet — that is exactly the gap I
am hoping to close. Treat it as a release candidate, keep your official WSJT-X
installed, and do not put it on a station you cannot afford to have misbehave.

Two other limitations worth knowing up front: a hold suppresses **every** station that
receives it, so there is no way yet to inhibit one radio and not another; and the UDP
port has no authentication, which is fine on a club LAN but means it should never be
exposed to the internet. The protocol can only *stop* a station transmitting — there
is no message that causes transmission — so the failure direction is at least the safe
one.

Bug reports, or anything confusing in the instructions:
<https://github.com/wa1hco/wsjtx-inhibit/issues>

Please do not report this to the WSJT-X developers — it is my fork, not their code,
and their time is better spent elsewhere.

73,
Jeff WA1HCO
