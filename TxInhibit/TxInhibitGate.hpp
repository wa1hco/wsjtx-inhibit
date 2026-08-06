#ifndef TX_INHIBIT_GATE_HPP__
#define TX_INHIBIT_GATE_HPP__

// ---------------------------------------------------------------------------
// TX Inhibit gate — pin filter for RTS/DTR PTT
//
// Concept (intentionally small):
//
//   physical PTT  =  WSJT-X intent  ∧  ¬ UDP hold
//
// • Intent arrives only from HamlibTransceiver::do_ptt() via set_intent().
// • Hold state is private (UDP KEY agent + deadman timer).
// • This class does NOT open a serial port. Hamlib owns CAT and RTS/DTR;
//   the gate only decides whether rig_set_ptt may assert the line.
//
// Lives as a child of HamlibTransceiver on the transceiver thread so
// set_intent and pin apply share that thread (stock CAT/Fake It order).
//
// Design authority: docs/TX_INHIBIT.md
// SPDX-License-Identifier: GPL-3.0-or-later
// ---------------------------------------------------------------------------

#include <QObject>
#include <QString>

#include "TxInhibitLogic.hpp"

class QUdpSocket;
class QTimer;

class TxInhibitGate
  : public QObject
{
  Q_OBJECT

public:
  explicit TxInhibitGate (QObject * parent = nullptr);
  ~TxInhibitGate () override;

public slots:
  // Bind UDP + start deadman timer (call once after construction on the
  // transceiver thread).
  void start_listening ();

  // WSJT-X TX intent from do_ptt(on). Does not take "hold" as an argument;
  // hold is private. Physical line is driven via physicalPtt(radiate).
  void set_intent (bool on);

  // Force intent off, stop UDP/timer (rig close / shutdown).
  // If emit_pin is false, do not request a physical PTT change (caller already
  // closed Hamlib or will set the pin itself). Safe to call more than once.
  void shutdown (bool emit_pin = true);

  // When true, ignore KEY-agent UDP (future UI test hold).
  void set_hold_test (bool hold);

signals:
  // Ask HamlibTransceiver to call rig_set_ptt (radiate true/false).
  // Same thread (DirectConnection) when parent is HamlibTransceiver.
  void physicalPtt (bool radiate);

  // Queued to GUI: status-bar badge.
  void inhibitChanged (bool inhibited, QString const& source);

  // Bound UDP port (22372 or ephemeral).
  void portBound (quint16 port);

  // Operator-visible problems (UDP bind, etc.).
  void lineError (QString const& message);

private slots:
  void on_udp_ready ();
  void tick ();

private:
  void ensure_udp ();
  void apply_line ();
  void emit_state_if_changed ();
  qint64 now_ms () const;

  TxInhibit::GateLogic logic_;
  QUdpSocket * udp_ {nullptr};
  QTimer * timer_ {nullptr};
  bool intent_ {false};
  bool hold_test_ {false};
  bool last_radiate_ {false};
  bool last_emitted_inhibited_ {false};
  bool stopped_ {false};         // after shutdown: no further pin emits
  QString last_badge_;
  quint16 bound_port_ {0};
};

#endif
