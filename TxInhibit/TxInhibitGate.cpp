#include "TxInhibitGate.hpp"

#include <QDateTime>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QtGlobal>

TxInhibitGate::TxInhibitGate (QObject * parent)
  : QObject {parent}
{
}

TxInhibitGate::~TxInhibitGate ()
{
  // Ensure PTT is low and ports closed even if Configuration forgot shutdown.
  shutdown ();
}

TxInhibitGate * TxInhibitGate::create_on_thread (QThread ** out_thread)
{
  auto * thread = new QThread;
  thread->setObjectName (QStringLiteral ("TxInhibitGateThread"));
  auto * gate = new TxInhibitGate;
  // Gate's event loop (UDP readyRead, timer, queued slots) runs on this thread.
  gate->moveToThread (thread);
  QObject::connect (thread, &QThread::finished, gate, &QObject::deleteLater);
  // Raise priority after start (best-effort; OS may ignore).
  QObject::connect (thread, &QThread::started, gate, [gate] () {
      QThread::currentThread ()->setPriority (QThread::TimeCriticalPriority);
      Q_UNUSED (gate);
    });
  thread->start ();
  if (out_thread)
    {
      *out_thread = thread;
    }
  return gate;
}

qint64 TxInhibitGate::now_ms () const
{
  // Wall-clock ms; same clock used for UDP hold deadlines.
  // (Monotonic would be nicer under NTP steps; wall clock matches simple agents.)
  return QDateTime::currentMSecsSinceEpoch ();
}

void TxInhibitGate::ensure_udp ()
{
  if (udp_)
    {
      return;
    }
  udp_ = new QUdpSocket (this);
  // Prefer well-known port 22372 so KEY agents can use a fixed default.
  // If busy, bind ephemeral and advertise via portBound (docs/TX_INHIBIT.md).
  // Construct QHostAddress explicitly so bind() is not ambiguous under C++11.
  QHostAddress const any4 {QHostAddress::AnyIPv4};
  if (!udp_->bind (any4, TxInhibit::default_gate_port,
                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
      udp_->bind (any4, quint16 (0));
    }
  bound_port_ = udp_->localPort ();
  QObject::connect (udp_, &QUdpSocket::readyRead, this, &TxInhibitGate::on_udp_ready);
  Q_EMIT portBound (bound_port_);

  if (!timer_)
    {
      timer_ = new QTimer (this);
      // 50 Hz is enough for deadman expiry; UDP assert is event-driven.
      timer_->setInterval (20);
      QObject::connect (timer_, &QTimer::timeout, this, &TxInhibitGate::tick);
      timer_->start ();
    }
}

void TxInhibitGate::configure (QString const& port_name, bool use_rts)
{
  // Always listen for KEY-agent UDP once the seat uses RTS/DTR PTT path.
  ensure_udp ();
  line_ = use_rts ? Line::RTS : Line::DTR;
  port_name_ = port_name;

  if (serial_)
    {
      serial_->close ();
      serial_->deleteLater ();
      serial_ = nullptr;
    }

  // PTT port must be a real device name (COMx / /dev/tty…). Shared USB CAT+RTS
  // uses that same real name for both. The list entry "CAT" means OmniRig-style
  // proxy (docs/TX_INHIBIT.md); gate needs a device path it can open.
  if (port_name.isEmpty () || port_name == QLatin1String ("None")
      || port_name == QLatin1String ("CAT"))
    {
      // Same brevity as other rig messages: title detail is in docs/TX_INHIBIT.md.
      Q_EMIT lineError (QStringLiteral ("TX Inhibit: invalid PTT port \"%1\"")
                        .arg (port_name.isEmpty () ? QStringLiteral ("(empty)") : port_name));
      apply_line ();
      emit_state_if_changed ();
      return;
    }

  serial_ = new QSerialPort (this);
  serial_->setPortName (port_name);
  // Modem lines only; baud is unused for RTS/DTR keying.
  // KEY sense is via UDP KEY agent (CTS left to the agent — docs/TX_INHIBIT.md).
  serial_->setBaudRate (QSerialPort::Baud9600);
  serial_->setDataBits (QSerialPort::Data8);
  serial_->setParity (QSerialPort::NoParity);
  serial_->setStopBits (QSerialPort::OneStop);
  serial_->setFlowControl (QSerialPort::NoFlowControl);
  QObject::connect (serial_,
#if QT_VERSION >= QT_VERSION_CHECK (5, 8, 0)
                    QOverload<QSerialPort::SerialPortError>::of (&QSerialPort::errorOccurred),
#else
                    static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)> (&QSerialPort::error),
#endif
                    this, &TxInhibitGate::on_serial_error);

  if (!serial_->open (QIODevice::ReadWrite))
    {
      Q_EMIT lineError (QStringLiteral ("TX Inhibit: failed to open \"%1\": %2")
                        .arg (port_name, serial_->errorString ()));
      serial_->deleteLater ();
      serial_ = nullptr;
      return;
    }

  // Safe idle: both handshake outputs low before we apply intent.
  serial_->setRequestToSend (false);
  serial_->setDataTerminalReady (false);
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::set_intent (bool on)
{
  // Intent is always accepted (sequencer thinks it is TXing). Radiation is
  // gated only in apply_line() when a UDP hold is active.
  intent_ = on;
  apply_line ();
}

void TxInhibitGate::set_hold_test (bool hold)
{
  // Future UI hook: when true, on_udp_ready ignores KEY-agent packets.
  hold_test_ = hold;
}

void TxInhibitGate::shutdown ()
{
  intent_ = false;
  if (serial_)
    {
      // Hard safe: both pins low before close.
      serial_->setRequestToSend (false);
      serial_->setDataTerminalReady (false);
      serial_->close ();
      serial_->deleteLater ();
      serial_ = nullptr;
    }
  if (udp_)
    {
      udp_->close ();
      udp_->deleteLater ();
      udp_ = nullptr;
      bound_port_ = 0;
    }
  if (timer_)
    {
      timer_->stop ();
      timer_->deleteLater ();
      timer_ = nullptr;
    }
}

void TxInhibitGate::on_udp_ready ()
{
  // Low-latency path: process KEY-agent datagrams and update the PTT pin
  // immediately (event-driven; tick() only covers deadman).
  if (!udp_ || hold_test_)
    {
      return;
    }
  while (udp_->hasPendingDatagrams ())
    {
      QByteArray data;
      data.resize (static_cast<int> (udp_->pendingDatagramSize ()));
      udp_->readDatagram (data.data (), data.size ());
      // on_datagram returns whether UDP-hold level flipped; we always recompute
      // radiate from intent + current hold (apply_line).
      (void) logic_.on_datagram (data, now_ms ());
    }
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::on_serial_error (QSerialPort::SerialPortError err)
{
  if (err == QSerialPort::NoError)
    {
      return;
    }
  Q_EMIT lineError (QStringLiteral ("TX Inhibit: serial error: %1")
                    .arg (serial_ ? serial_->errorString () : QStringLiteral ("unknown")));
}

void TxInhibitGate::tick ()
{
  // Run deadman expiry even when no UDP traffic (inhibited() clears deadline).
  (void) logic_.inhibited (now_ms ());
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::apply_line ()
{
  // Sole place that sets the physical PTT pin for this seat:
  // radiate = intent ∧ open hold window (UDP KEY agent).
  bool radiate = intent_ && !logic_.line_inhibited (now_ms ());
  if (!serial_ || !serial_->isOpen ())
    {
      return;
    }
  if (line_ == Line::RTS)
    {
      serial_->setRequestToSend (radiate);
    }
  else
    {
      serial_->setDataTerminalReady (radiate);
    }
}

void TxInhibitGate::emit_state_if_changed ()
{
  // Badge follows line_inhibited (PTT blocked while UDP hold is active).
  qint64 t = now_ms ();
  bool inh = logic_.line_inhibited (t);
  auto badge = logic_.badge_text (t);
  if (inh != last_emitted_inhibited_ || badge != last_badge_)
    {
      last_emitted_inhibited_ = inh;
      last_badge_ = badge;
      Q_EMIT inhibitChanged (inh, badge);
    }
}
