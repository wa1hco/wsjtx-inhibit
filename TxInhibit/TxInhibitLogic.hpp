#ifndef TX_INHIBIT_LOGIC_HPP__
#define TX_INHIBIT_LOGIC_HPP__

// ---------------------------------------------------------------------------
// Pure, I/O-free TX Inhibit gate logic
//
// No sockets, no serial I/O — only parse + state. Easy to unit-test and to
// keep in sync with KEY-agent senders.
//
// Design authority: docs/TX_INHIBIT.md
//
// Mental model (do not conflate):
//
//   1. Datagram fields (ttl_ms, station, …)
//        What the KEY agent *requested* in one UDP packet.
//
//   2. inhibited(now)  — "UDP hold active?"
//        True while an unexpired hold deadline is armed.
//
//   3. line_inhibited(now)  — "must the radio PTT stay low?"
//        Today: same as inhibited(now) (UDP only).
//        Local CTS KEY is out of the seat binary (spurious CTS on real
//        COM ports); if re-added later, gate it with Settings and OR here.
//        apply_line: radiate = intent && !line_inhibited(now)
//
// Hang for CW/break-in gaps is KEY-agent policy (not in this process).
//
// SPDX-License-Identifier: GPL-3.0-or-later
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <limits>

namespace TxInhibit {

// Project builds with --std=gnu++11 (-Werror); keep this header C++11-clean.
static constexpr char const * protocol_key = "tx_inhibit";
static constexpr int protocol_version = 1;
// Well-known gate listen / agent send port (IPv4). Ephemeral fallback is
// handled by TxInhibitGate if bind fails.
static constexpr quint16 default_gate_port = 22372;
// Non-zero ttl_ms must land in this range (0 is reserved for release).
static constexpr int ttl_ms_min = 100;
static constexpr int ttl_ms_max = 30000;
static constexpr int max_datagram_bytes = 512;

// Parsed view of one KEY-agent UDP JSON body.
// valid == false means "ignore this packet; do not change hold state."
struct Datagram
{
  int ttl_ms {0};       // >0 hold/refresh for that many ms; 0 = release
  QString station;      // badge: "held by …"
  QString band;         // informational today (gate does not filter by band)
  qint64 seq {0};       // informational agent sequence
  bool valid {false};
};

// Validate and unpack one UDP payload. On any failure returns valid=false
// (empty Datagram) so callers can count invalids without partial updates.
inline Datagram parse_datagram (QByteArray const& data)
{
  Datagram out;
  if (data.isEmpty () || data.size () > max_datagram_bytes)
    {
      return out;
    }
  QJsonParseError err;
  auto doc = QJsonDocument::fromJson (data, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject ())
    {
      return out;
    }
  auto obj = doc.object ();
  // Protocol id must be exactly protocol_version (currently 1).
  if (obj.value (QLatin1String (protocol_key)).toInt (-1) != protocol_version)
    {
      return out;
    }
  // Accept JSON number (Qt stores as Double) or string digits (interop).
  if (!obj.contains (QLatin1String ("ttl_ms")))
    {
      return out;
    }
  {
    auto const ttl_v = obj.value (QLatin1String ("ttl_ms"));
    if (!ttl_v.isDouble () && !ttl_v.isString ())
      {
        return out;
      }
  }
  int ttl = obj.value (QLatin1String ("ttl_ms")).toInt (-1);
  // 0 = release; otherwise enforce min/max so deadman windows stay sane.
  if (ttl != 0 && (ttl < ttl_ms_min || ttl > ttl_ms_max))
    {
      return out;
    }
  out.ttl_ms = ttl;
  out.station = obj.value (QLatin1String ("station")).toString ();
  out.band = obj.value (QLatin1String ("band")).toString ();
  out.seq = static_cast<qint64> (obj.value (QLatin1String ("seq")).toDouble (0));
  out.valid = true;
  return out;
}

// One UDP hold at a time: a single deadline. Last valid hold wins.
// now_ms is supplied by the caller (same time base for deadline math).
class GateLogic
{
public:
  // Apply one hold/release packet to the UDP-hold state machine.
  //
  // Updates the hold *deadline* from the datagram; does not touch serial
  // lines or TX intent (those live in TxInhibitGate).
  //
  // Return value (important):
  //   true  = the UDP-hold *level* flipped (free↔held), i.e.
  //           inhibited(now) after != inhibited(now) before.
  //   false = level unchanged — including keepalives that only refresh
  //           the deadline, invalid packets, and redundant releases.
  //
  // This is NOT "datagram says hold" (a hold while already held returns
  // false). It is NOT "RTS/DTR changed" (see line_inhibited / apply_line).
  // Callers may ignore the return and re-evaluate line_inhibited() instead;
  // TxInhibitGate does exactly that today.
  bool on_datagram (QByteArray const& data, qint64 now_ms)
  {
    auto msg = parse_datagram (data);
    if (!msg.valid)
      {
        ++invalid_;
        return false; // no state change
      }
    // Snapshot UDP-hold level *before* mutating the deadline.
    bool before = inhibited (now_ms);
    if (msg.ttl_ms == 0)
      {
        // Explicit release from KEY agent (preferred over waiting for deadman).
        ++release_rx_;
        deadline_ms_ = -1;
      }
    else
      {
        // Hold or keepalive: re-arm deadman window from *this* packet's time.
        ++hold_rx_;
        deadline_ms_ = now_ms + msg.ttl_ms;
        holder_ = msg.station;
      }
    return inhibited (now_ms) != before;
  }

  // UDP-hold active? Also runs deadman: if deadline has passed, clear it
  // and count an expiry. Side effect on expiry is intentional so a 50 Hz
  // poll can open the band without another packet.
  bool inhibited (qint64 now_ms)
  {
    if (deadline_ms_ >= 0 && now_ms >= deadline_ms_)
      {
        deadline_ms_ = -1;
        ++expiries_;
      }
    return deadline_ms_ >= 0;
  }

  // Station string for the badge while UDP-held; empty if free.
  QString holding_station (qint64 now_ms)
  {
    return inhibited (now_ms) ? holder_ : QString {};
  }

  // True => physical PTT key line must stay low (regardless of TX intent).
  // UDP hold only (no local CTS in this build).
  bool line_inhibited (qint64 now_ms)
  {
    return inhibited (now_ms);
  }

  // Pin formula used by the gate (unit-test friendly).
  // radiate = intent && !line_inhibited(now)
  bool radiate (bool intent, qint64 now_ms)
  {
    return intent && !line_inhibited (now_ms);
  }

  // Status-bar text; empty string means "not inhibited" (hide the badge).
  QString badge_text (qint64 now_ms)
  {
    if (inhibited (now_ms))
      {
        auto s = holding_station (now_ms);
        if (s.isEmpty ())
          {
            return QStringLiteral ("TX INHIBITED");
          }
        return QStringLiteral ("TX INHIBITED — held by %1").arg (s);
      }
    return {};
  }

  // Diagnostics (optional telemetry / InhibitStatus).
  quint32 hold_rx () const { return hold_rx_; }
  quint32 release_rx () const { return release_rx_; }
  quint32 expiries () const { return expiries_; }
  quint32 invalid () const { return invalid_; }

private:
  // UDP hold deadline in the same ms epoch as now_ms arguments.
  // -1 means no UDP hold (band free from the UDP path).
  qint64 deadline_ms_ {-1};
  QString holder_;           // last hold's station field
  quint32 hold_rx_ {0};
  quint32 release_rx_ {0};
  quint32 expiries_ {0};
  quint32 invalid_ {0};
};

} // namespace TxInhibit

#endif
