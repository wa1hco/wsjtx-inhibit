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
  shutdown ();
}

TxInhibitGate * TxInhibitGate::create_on_thread (QThread ** out_thread)
{
  auto * thread = new QThread;
  thread->setObjectName (QStringLiteral ("TxInhibitGateThread"));
  auto * gate = new TxInhibitGate;
  gate->moveToThread (thread);
  QObject::connect (thread, &QThread::finished, gate, &QObject::deleteLater);
  // Raise priority after start (best-effort; may be ignored).
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
  return QDateTime::currentMSecsSinceEpoch ();
}

void TxInhibitGate::ensure_udp ()
{
  if (udp_)
    {
      return;
    }
  udp_ = new QUdpSocket (this);
  // Prefer 22372; fall back to ephemeral (§11.2).
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
      timer_->setInterval (20); // 50 Hz: deadman + CTS poll
      QObject::connect (timer_, &QTimer::timeout, this, &TxInhibitGate::tick);
      timer_->start ();
    }
}

void TxInhibitGate::configure (QString const& port_name, bool use_rts)
{
  ensure_udp ();
  line_ = use_rts ? Line::RTS : Line::DTR;
  port_name_ = port_name;

  if (serial_)
    {
      serial_->close ();
      serial_->deleteLater ();
      serial_ = nullptr;
    }

  if (port_name.isEmpty () || port_name == QLatin1String ("None")
      || port_name == QLatin1String ("CAT"))
    {
      apply_line ();
      emit_state_if_changed ();
      return;
    }

  serial_ = new QSerialPort (this);
  serial_->setPortName (port_name);
  // Modem lines only; baud is irrelevant for RTS/DTR/CTS.
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
      Q_EMIT lineError (QStringLiteral ("TxInhibit: cannot open %1: %2")
                        .arg (port_name, serial_->errorString ()));
      serial_->deleteLater ();
      serial_ = nullptr;
      return;
    }

  // Idle: deassert PTT lines.
  serial_->setRequestToSend (false);
  serial_->setDataTerminalReady (false);
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::set_intent (bool on)
{
  intent_ = on;
  apply_line ();
}

void TxInhibitGate::set_hold_test (bool hold)
{
  hold_test_ = hold;
}

void TxInhibitGate::shutdown ()
{
  intent_ = false;
  if (serial_)
    {
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
  if (!udp_ || hold_test_)
    {
      return;
    }
  while (udp_->hasPendingDatagrams ())
    {
      QByteArray data;
      data.resize (static_cast<int> (udp_->pendingDatagramSize ()));
      udp_->readDatagram (data.data (), data.size ());
      logic_.on_datagram (data, now_ms ());
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
  Q_EMIT lineError (QStringLiteral ("TxInhibit serial: %1")
                    .arg (serial_ ? serial_->errorString () : QStringLiteral ("unknown")));
}

void TxInhibitGate::poll_cts ()
{
  if (!serial_ || !serial_->isOpen ())
    {
      return;
    }
  auto pins = serial_->pinoutSignals ();
  bool cts = pins & QSerialPort::ClearToSendSignal;
  qint64 t = now_ms ();
  if (cts)
    {
      last_cts_raw_ = true;
      cts_release_at_ms_ = -1;
      logic_.set_local_cts (true);
    }
  else if (last_cts_raw_)
    {
      // Edge: KEY opened — start hang (fixed 0.5 s for raw KEY; UDP hang is agent-side).
      last_cts_raw_ = false;
      cts_release_at_ms_ = t + cts_hang_ms_;
    }
  if (cts_release_at_ms_ >= 0)
    {
      if (t >= cts_release_at_ms_)
        {
          cts_release_at_ms_ = -1;
          logic_.set_local_cts (false);
        }
      else
        {
          logic_.set_local_cts (true); // still in hang
        }
    }
}

void TxInhibitGate::tick ()
{
  poll_cts ();
  // Expire UDP deadman.
  (void) logic_.inhibited (now_ms ());
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::apply_line ()
{
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
