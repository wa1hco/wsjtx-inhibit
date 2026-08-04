# Moved

TX Inhibit and KEY agent design for this repository live in:

**[TX_INHIBIT.md](TX_INHIBIT.md)**

That document is self-contained. A KEY agent is any program that senses a
priority radio’s KEY line and sends the documented UDP hold/keepalive/release
datagrams to the seat gate. Product-specific multi-op frameworks are optional
clients of the same protocol; they are not required to understand this code.

**Operators — shared USB CAT + RTS/DTR (common pitfalls):**  
see [TX_INHIBIT.md § Shared USB CAT + RTS/DTR](TX_INHIBIT.md#shared-usb-cat--rtsdtr-what-operators-actually-do).
