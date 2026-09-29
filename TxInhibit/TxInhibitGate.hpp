#ifndef TX_INHIBIT_GATE_HPP__
#define TX_INHIBIT_GATE_HPP__

// ---------------------------------------------------------------------------
// TxInhibit module — UDP listen + want_tx mix for RTS/DTR PTT
//
//   assert PTT  ⇔  want_tx  and  not hold
//
// • want_tx arrives from HamlibTransceiver::do_ptt() via set_intent().
// • Hold is private (per-controller leases OR'd + lease TTL deadman).
//   CW anti-chatter hang lives only in the KEY agent; normal end is
//   release of that controller's lease (TTL 0). See docs/TX_INHIBIT.md §3.
// • No serial I/O here. Hamlib owns CAT and RTS/DTR; this only decides
//   whether to assert PTT or release PTT on the pin.
// • WSJT-X station = this WSJT-X station (app + PC + radio + antenna).
//
// Child of HamlibTransceiver on the transceiver thread (stock CAT order).
//
// Design authority / glossary: docs/TX_INHIBIT.md
// SPDX-License-Identifier: GPL-3.0-or-later
// ---------------------------------------------------------------------------

#include <atomic>

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include "TxInhibitLogic.hpp"

class QTimer;

class TxInhibitGate
  : public QObject
{
  Q_OBJECT

public:
  explicit TxInhibitGate (QObject * parent = nullptr);
  ~TxInhibitGate () override;

  // NetworkMessage Id for this station (usually QApplication::applicationName()).
  // Type-18 datagrams with a non-empty target Id must match; empty Id = any.
  void set_instance_id (QString const& id);

  // Accept UDP requests. False ignores new type 18. Leases already running
  // still expire and still hold the pin.
  static void set_commands_enabled (bool enabled);

  // CLOCK_MONOTONIC just after the RTS/DTR ioctl that released the line.
  void note_pin_off_ns (qint64 ns);

  // Type 18 that arrived on the shared WSJT-X UDP socket (heartbeat source),
  // from the dispatch thread. No-op when no gate is published or commands
  // are disabled. A datagram that would open the first lease sets
  // hold_pending() before the pin work is queued onto this thread. A refresh
  // of an existing lease does not.
  // t_rx_ns is CLOCK_MONOTONIC at the socket read. Zero skips the pin log.
  static void submit_shared (QByteArray const& data, qint64 t_rx_ns = 0);

  // True from submit_shared of an initiating hold until that datagram has
  // been applied on this thread. HamlibTransceiver::do_poll returns early
  // while this is set so the pin drop is not stuck behind a meter read.
  bool hold_pending () const
  {
    return pending_.load (std::memory_order_acquire);
  }

public slots:
  // Bind UDP + start hold-timeout poll timer (once on transceiver thread).
  void start_listening ();

  // WSJT-X TX intent from do_ptt(on). Does not take "hold" as an argument;
  // hold is private. Physical line is driven via physicalPtt(radiate).
  void set_intent (bool on);

  // Force intent off, stop UDP/timer (rig close / shutdown).
  // If emit_pin is false, do not request a physical PTT change (caller already
  // closed Hamlib or will set the pin itself). Safe to call more than once.
  void shutdown (bool emit_pin = true);

signals:
  // Ask HamlibTransceiver to call rig_set_ptt (radiate true/false).
  // Same thread (DirectConnection) when parent is HamlibTransceiver.
  void physicalPtt (bool radiate);

  // Queued to GUI: status-bar badge + optional InhibitStatus counters.
  // t_rx_ns and t_pin_ns are CLOCK_MONOTONIC. Both are zero unless this
  // emission is the hold that dropped the line. t_pin_ns is the modem-line
  // ioctl, not the return from rig_set_ptt.
  void inhibitChanged (bool inhibited, QString const& source
                       , quint32 hold_rx, quint32 release_rx
                       , quint32 expiries, quint32 invalid
                       , qint64 t_rx_ns, qint64 t_pin_ns);

  // Bound UDP inhibit listen port (always ephemeral / OS-assigned).
  // Never emitted with port 0: that is not a usable KEY-agent target.
  void portBound (quint16 port);

  // Non-fatal operator-visible problems (e.g. total UDP bind failure).
  // Callers must not treat this as a rig CAT/PTT failure.
  void lineError (QString const& message);

  // rig_set_ptt failed while the gate was driving the pin. Unlike lineError
  // (bind problems), this should surface as a rig/operator failure so the UI
  // does not show "Tx" with no RF and no message.
  void pttApplyFailed (QString const& message);

private slots:
  void tick ();
  // Type 18 from the shared UDP socket. Runs on the transceiver thread.
  void ingest_shared (QByteArray data);

private:
  void apply_line ();
  // Emits physicalPtt with exceptions contained. The slot on the other end
  // calls rig_set_ptt, which throws; this is reached from timer and socket
  // slots where an escaping exception would abort the application.
  void emit_physical_ptt (bool radiate);
  void emit_state_if_changed (qint64 t_rx_ns = 0, qint64 t_pin_ns = 0);
  void publish_inhibited ();
  qint64 now_ms () const;

  // Monotonic time base for hold expiry: immune to system-clock steps, which
  // WSJT-X hosts take routinely from time-sync tools. See now_ms().
  QElapsedTimer uptime_;
  TxInhibit::GateLogic logic_;
  QTimer * timer_ {nullptr};
  bool intent_ {false};
  bool last_radiate_ {false};
  bool last_emitted_inhibited_ {false};
  bool stopped_ {false};         // after shutdown: no further pin emits
  QString last_badge_;
  // Published for the dispatch thread. True while any lease is unexpired.
  std::atomic<bool> inhibited_published_ {false};
  // Set on the dispatch thread for an initiating hold; cleared on this thread
  // after ingest_shared has applied the pin decision.
  std::atomic<bool> pending_ {false};
  // CLOCK_MONOTONIC ns when the initiating type 18 left the socket.
  std::atomic<qint64> t_rx_ns_ {0};
  std::atomic<qint64> t_pin_ns_ {0};
};

#endif
