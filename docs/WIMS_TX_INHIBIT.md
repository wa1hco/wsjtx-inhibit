# Moved

TX Inhibit and KEY agent design for this repository live in:

**[TX_INHIBIT.md](TX_INHIBIT.md)**

That document is self-contained. A **KEY agent** (the role) senses a priority
radio’s KEY and sends UDP hold/keepalive/release hold to each WSJT-X station.
This tree’s standalone program is **`inhibit-agent`** (operator setup). The WIMS
tree’s program is **`wims-key-agent`** (destinations from WIMS discovery).

**Operators — shared USB CAT + RTS/DTR (common pitfalls):**  
see [TX_INHIBIT.md § Shared USB CAT + RTS/DTR](TX_INHIBIT.md#shared-usb-cat--rtsdtr).
