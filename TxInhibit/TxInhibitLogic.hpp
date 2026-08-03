#ifndef TX_INHIBIT_LOGIC_HPP__
#define TX_INHIBIT_LOGIC_HPP__

// Pure, I/O-free inhibit gate logic for WSJT-X WIMS.
// Transplant of WIMS src/wims/interlock/inhibit.py (InhibitGate + parse).
// Design: WIMS docs/plan/wims_tx_inhibit.md §3 / §11
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <limits>

namespace TxInhibit {

inline constexpr char const * protocol_key = "tx_inhibit";
inline constexpr int protocol_version = 1;
inline constexpr quint16 default_gate_port = 22372;
inline constexpr int ttl_ms_min = 100;
inline constexpr int ttl_ms_max = 30000;
inline constexpr int max_datagram_bytes = 512;

struct Datagram
{
  int ttl_ms {0};
  QString station;
  QString band;
  qint64 seq {0};
  bool valid {false};
};

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
  if (obj.value (QLatin1String (protocol_key)).toInt (-1) != protocol_version)
    {
      return out;
    }
  if (!obj.contains (QLatin1String ("ttl_ms")) || !obj.value (QLatin1String ("ttl_ms")).isDouble ())
    {
      return out;
    }
  int ttl = obj.value (QLatin1String ("ttl_ms")).toInt (-1);
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

// One hold, one deadline (monotonic ms). Last datagram wins; ttl_ms==0 releases.
class GateLogic
{
public:
  // Returns true if inhibited output changed.
  bool on_datagram (QByteArray const& data, qint64 now_ms)
  {
    auto msg = parse_datagram (data);
    if (!msg.valid)
      {
        ++invalid_;
        return false;
      }
    bool before = inhibited (now_ms);
    if (msg.ttl_ms == 0)
      {
        ++release_rx_;
        deadline_ms_ = -1;
      }
    else
      {
        ++hold_rx_;
        deadline_ms_ = now_ms + msg.ttl_ms;
        holder_ = msg.station;
      }
    return inhibited (now_ms) != before;
  }

  bool inhibited (qint64 now_ms)
  {
    if (deadline_ms_ >= 0 && now_ms >= deadline_ms_)
      {
        deadline_ms_ = -1;
        ++expiries_;
      }
    return deadline_ms_ >= 0;
  }

  QString holding_station (qint64 now_ms)
  {
    return inhibited (now_ms) ? holder_ : QString {};
  }

  // Local CTS level (already hung by caller if needed). OR with UDP hold.
  void set_local_cts (bool keyed)
  {
    local_cts_ = keyed;
  }

  bool local_cts () const { return local_cts_; }

  bool line_inhibited (qint64 now_ms)
  {
    return local_cts_ || inhibited (now_ms);
  }

  QString badge_text (qint64 now_ms)
  {
    if (local_cts_)
      {
        return QStringLiteral ("TX INHIBITED — local KEY line");
      }
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

  quint32 hold_rx () const { return hold_rx_; }
  quint32 release_rx () const { return release_rx_; }
  quint32 expiries () const { return expiries_; }
  quint32 invalid () const { return invalid_; }

private:
  qint64 deadline_ms_ {-1};
  QString holder_;
  bool local_cts_ {false};
  quint32 hold_rx_ {0};
  quint32 release_rx_ {0};
  quint32 expiries_ {0};
  quint32 invalid_ {0};
};

} // namespace TxInhibit

#endif
