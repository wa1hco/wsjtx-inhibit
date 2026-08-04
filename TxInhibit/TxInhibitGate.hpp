#ifndef TX_INHIBIT_GATE_HPP__
#define TX_INHIBIT_GATE_HPP__

// PTT gate thread: RTS/DTR = intent ∧ ¬inhibit.
// Design: WIMS docs/plan/wims_tx_inhibit.md §5 / §11
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QObject>
#include <QString>
#include <QSerialPort>

#include "TxInhibitLogic.hpp"

class QUdpSocket;
class QTimer;
class QThread;

// Lives on its own QThread (moveToThread). All public slots are thread-safe
// entry points via queued connections from the GUI / Configuration thread.
class TxInhibitGate
  : public QObject
{
  Q_OBJECT

public:
  enum class Line { RTS, DTR };

  explicit TxInhibitGate (QObject * parent = nullptr);
  ~TxInhibitGate () override;

  // Convenience: create worker + thread, return worker pointer (owned by thread).
  static TxInhibitGate * create_on_thread (QThread ** out_thread);

public slots:
  // Open serial PTT port (same port's CTS is the local KEY input). Empty port
  // means UDP-only gate (still binds inhibit socket; no line drive).
  void configure (QString const& port_name, bool use_rts /* else DTR */);

  // TX intent from WSJT-X (always acknowledged; line may stay low if inhibited).
  void set_intent (bool on);

  // Force line deassert and close (settings change / shutdown).
  void shutdown ();

  // Optional: hold datagrams (for "TX inhibit test" menu — future).
  void set_hold_test (bool hold);

signals:
  // GUI thread: badge + InhibitStatus.
  void inhibitChanged (bool inhibited, QString const& source);
  // Bound UDP port for InhibitStatus (0 if not bound).
  void portBound (quint16 port);
  void lineError (QString const& message);

private slots:
  void on_udp_ready ();
  void on_serial_error (QSerialPort::SerialPortError);
  void tick ();

private:
  void ensure_udp ();
  void apply_line ();
  void poll_cts ();
  void emit_state_if_changed ();
  qint64 now_ms () const;

  TxInhibit::GateLogic logic_;
  QUdpSocket * udp_ {nullptr};
  QSerialPort * serial_ {nullptr};
  QTimer * timer_ {nullptr};
  QString port_name_;
  Line line_ {Line::RTS};
  bool intent_ {false};
  bool hold_test_ {false};
  bool last_emitted_inhibited_ {false};
  QString last_badge_;
  quint16 bound_port_ {0};
  bool last_cts_raw_ {false};
  qint64 cts_down_at_ms_ {-1};     // when current KEY-down (CTS high) started
  qint64 cts_release_at_ms_ {-1};  // hang deadline after KEY-up
  // Do not treat CTS as KEY until we have seen it deasserted once after open.
  // Floating CTS-high on many USB-serial adapters would otherwise stick inhibit.
  bool cts_armed_ {false};
  // Recent KEY-down durations (ms) for WIMS adaptive hang; newest last.
  int closure_ms_[TxInhibit::closure_window] {};
  int closure_count_ {0};

  void push_closure_ms (int duration_ms);
  int current_hang_ms () const;
};

#endif
