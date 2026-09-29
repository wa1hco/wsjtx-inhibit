# Apply notes — WS 3.2.1 260926

Patch: `tx-inhibit-ws-3.2.1_260926.patch`
SHA-256: `46a4510207afb8fbbc165586f4f3d6cdd699e4eb31c88718d4656d704cf2b4b0`
Size: 80869 bytes
Mail body: `../../docs/announce-ws-3.2.1-260926.md`

The patch is against the inner `ws.tgz` of `ws-3.2.1_260926.tgz`, not against
the superbuild wrapper. A dry run of `patch -p1 --binary` on that tree
reported every file checking clean.

```text
tar xzf ws-3.2.1_260926.tgz
tar xzf ws-3.2.1/src/ws.tgz
cd ws
patch -p1 --binary < tx-inhibit-ws-3.2.1_260926.patch
```

`--binary` matters when line endings disagree. The WS sources are CRLF.
The three new files in the patch (`Network/UdpDispatch.cpp`,
`Network/UdpDispatch.hpp`, `TxInhibit/TxInhibitDrop.hpp`) are CRLF as well.
