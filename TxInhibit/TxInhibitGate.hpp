#ifndef TX_INHIBIT_GATE_HPP__
#define TX_INHIBIT_GATE_HPP__

// ---------------------------------------------------------------------------
// TX Inhibit gate — I/O thread
//
// Owns the seat's serial PTT (RTS or DTR) and combines:
//   • TX intent from WSJT-X (sequencer / Test PTT)
//   • UDP holds from a KEY agent (or inhibit-spacebar / send_inhibit_hold.py)
//
// Physical line:
//   RTS or DTR  =  intent  ∧  ¬ inhibited(UDP hold)
//
// KEY sense and hang live in the KEY agent (UDP). The gate opens RTS/DTR only.
// (A future optional Settings-gated CTS path would be opt-in; default is UDP.)
//
// WSJT-X audio and FT8 sequencing continue; the gate only holds the PTT pin.
//
// Lives on its own QThread (see create_on_thread). Public slots are the
// cross-thread API (QueuedConnection from Configuration / GUI).
//
// Design authority: docs/TX_INHIBIT.md
// SPDX-License-Identifier: GPL-3.0-or-later
// ---------------------------------------------------------------------------

#include <QObject>
#include <QString>
#include <QSerialPort>

#include "TxInhibitLogic.hpp"

class QUdpSocket;
class QTimer;
class QThread;

class TxInhibitGate
  : public QObject
{
  Q_OBJECT

public:
  // Which modem output pin keys the radio (matches Settings → PTT method).
  enum class Line { RTS, DTR };

  explicit TxInhibitGate (QObject * parent = nullptr);
  ~TxInhibitGate () override;

  // Create gate + QThread, move gate onto the thread, start it.
  // Returns the worker (owned by the thread; deleted on thread finish).
  // *out_thread receives the thread pointer for quit/wait/delete by Configuration.
  static TxInhibitGate * create_on_thread (QThread ** out_thread);

public slots:
  // (Re)open the serial PTT port and choose RTS vs DTR.
  // port_name: real device (may match CAT COM for shared USB RTS/DTR).
  // Empty / "None" / list entry "CAT" → UDP listener only until a real port is set.
  void configure (QString const& port_name, bool use_rts /* else DTR */);

  // WSJT-X TX intent. Always stored; physical pin stays low during a UDP hold.
  // Sequencing and audio keep running; only the PTT pin is gated.
  void set_intent (bool on);

  // Deassert PTT, close serial + UDP, stop timer (settings change / shutdown).
  void shutdown ();

  // When true, on_udp_ready skips KEY-agent packets (future UI test hold).
  void set_hold_test (bool hold);

signals:
  // Queued to GUI: show/hide status-bar badge.
  // inhibited = UDP hold active; source = badge text (empty when open).
  void inhibitChanged (bool inhibited, QString const& source);

  // Actual bound UDP port after ensure_udp (22372 or ephemeral).
  void portBound (quint16 port);

  // Operator-visible serial / config problems (surfaced as transceiver_failure).
  void lineError (QString const& message);

private slots:
  // Hot path: KEY-agent packets → GateLogic → apply_line (event-driven).
  void on_udp_ready ();
  void on_serial_error (QSerialPort::SerialPortError);
  // 50 Hz: UDP deadman expiry + re-apply line + badge.
  void tick ();

private:
  void ensure_udp ();
  // Drive RTS/DTR from intent_ and UDP hold (line_inhibited).
  void apply_line ();
  // Emit inhibitChanged when hold level or badge text changes.
  void emit_state_if_changed ();
  qint64 now_ms () const;

  TxInhibit::GateLogic logic_;
  QUdpSocket * udp_ {nullptr};
  QSerialPort * serial_ {nullptr};
  QTimer * timer_ {nullptr};
  QString port_name_;
  Line line_ {Line::RTS};
  bool intent_ {false};          // last set_intent from WSJT-X
  bool hold_test_ {false};       // if true, ignore UDP (see set_hold_test)
  bool last_emitted_inhibited_ {false};
  QString last_badge_;
  quint16 bound_port_ {0};
};

#endif
