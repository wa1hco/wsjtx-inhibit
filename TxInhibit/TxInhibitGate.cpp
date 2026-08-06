#include "TxInhibitGate.hpp"

#include <QDateTime>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

TxInhibitGate::TxInhibitGate (QObject * parent)
  : QObject {parent}
{
}

TxInhibitGate::~TxInhibitGate ()
{
  // Never emit physicalPtt from the destructor — Hamlib may already be closed.
  shutdown (false);
}

qint64 TxInhibitGate::now_ms () const
{
  return QDateTime::currentMSecsSinceEpoch ();
}

void TxInhibitGate::start_listening ()
{
  stopped_ = false;
  ensure_udp ();
}

void TxInhibitGate::ensure_udp ()
{
  if (udp_ || stopped_)
    {
      return;
    }
  udp_ = new QUdpSocket (this);
  // Prefer well-known port 22372 so KEY agents can use a fixed default.
  QHostAddress const any4 {QHostAddress::AnyIPv4};
  if (!udp_->bind (any4, TxInhibit::default_gate_port,
                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
      if (!udp_->bind (any4, quint16 (0)))
        {
          Q_EMIT lineError (QStringLiteral ("TX Inhibit: UDP bind failed: %1")
                            .arg (udp_->errorString ()));
          udp_->deleteLater ();
          udp_ = nullptr;
          return;
        }
    }
  bound_port_ = udp_->localPort ();
  QObject::connect (udp_, &QUdpSocket::readyRead, this, &TxInhibitGate::on_udp_ready);
  Q_EMIT portBound (bound_port_);

  if (!timer_)
    {
      timer_ = new QTimer (this);
      // Deadman expiry; UDP assert is event-driven.
      timer_->setInterval (20);
      QObject::connect (timer_, &QTimer::timeout, this, &TxInhibitGate::tick);
      timer_->start ();
    }
}

void TxInhibitGate::set_intent (bool on)
{
  if (stopped_)
    {
      return;
    }
  intent_ = on;
  apply_line ();
}

void TxInhibitGate::set_hold_test (bool hold)
{
  hold_test_ = hold;
}

void TxInhibitGate::shutdown (bool emit_pin)
{
  stopped_ = true;
  intent_ = false;

  if (emit_pin && last_radiate_)
    {
      // Request pin low once while Hamlib is still open (caller responsibility).
      last_radiate_ = false;
      Q_EMIT physicalPtt (false);
    }
  else
    {
      last_radiate_ = false;
    }

  if (timer_)
    {
      timer_->stop ();
      timer_->deleteLater ();
      timer_ = nullptr;
    }
  if (udp_)
    {
      udp_->disconnect (this);
      udp_->close ();
      udp_->deleteLater ();
      udp_ = nullptr;
      bound_port_ = 0;
    }
}

void TxInhibitGate::on_udp_ready ()
{
  if (!udp_ || hold_test_ || stopped_)
    {
      return;
    }
  while (udp_->hasPendingDatagrams ())
    {
      QByteArray data;
      data.resize (static_cast<int> (udp_->pendingDatagramSize ()));
      udp_->readDatagram (data.data (), data.size ());
      (void) logic_.on_datagram (data, now_ms ());
    }
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::tick ()
{
  if (stopped_)
    {
      return;
    }
  (void) logic_.inhibited (now_ms ());
  apply_line ();
  emit_state_if_changed ();
}

void TxInhibitGate::apply_line ()
{
  if (stopped_)
    {
      return;
    }
  // Sole policy: radiate = intent ∧ ¬UDP hold.
  bool const radiate = intent_ && !logic_.line_inhibited (now_ms ());
  if (radiate == last_radiate_)
    {
      return;
    }
  last_radiate_ = radiate;
  Q_EMIT physicalPtt (radiate);
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
