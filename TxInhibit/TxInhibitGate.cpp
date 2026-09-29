#include "TxInhibitGate.hpp"

#include <exception>
#include <cstdio>

#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>

namespace
{
  QMutex& gate_mu ()
  {
    static QMutex mu;
    return mu;
  }

  TxInhibitGate *& current_gate ()
  {
    static TxInhibitGate * gate = nullptr;
    return gate;
  }

  // Accept UDP requests. Off ignores new type 18. Live leases still expire.
  std::atomic<bool>& commands_enabled ()
  {
    static std::atomic<bool> enabled {false};
    return enabled;
  }

  // One line per actual RTS/DTR release. dt is socket read until the ioctl.
  // until rig_set_ptt(OFF) returns, not the type-17 GUI announce.
  void log_pin_release (qint64 dt_us)
  {
    std::fprintf (stderr, "TXINHIBIT_PIN dt_us=%lld\n",
                  static_cast<long long> (dt_us));
    std::fflush (stderr);
    if (FILE * out = std::fopen ("/home/jeff/ham/wsjtx-32rc1-latency/pin-dt.log", "a"))
      {
        std::fprintf (out, "%lld\n", static_cast<long long> (dt_us));
        std::fclose (out);
      }
  }
}

TxInhibitGate::TxInhibitGate (QObject * parent)
  : QObject {parent}
{
  uptime_.start ();
}

void TxInhibitGate::set_instance_id (QString const& id)
{
  QMutexLocker lock (&gate_mu ());
  logic_.set_instance_id (id);
}

void TxInhibitGate::set_commands_enabled (bool enabled)
{
  commands_enabled ().store (enabled, std::memory_order_release);
}

void TxInhibitGate::note_pin_off_ns (qint64 ns)
{
  t_pin_ns_.store (ns, std::memory_order_release);
}

void TxInhibitGate::submit_shared (QByteArray const& data, qint64 t_rx_ns)
{
  // Hold the mutex through the queued post. shutdown() takes the same mutex
  // before deleteLater, so the ingest event is ordered ahead of destruction.
  QMutexLocker lock (&gate_mu ());
  if (!commands_enabled ().load (std::memory_order_acquire))
    {
      return;
    }
  TxInhibitGate * gate = current_gate ();
  if (!gate || gate->stopped_)
    {
      return;
    }
  auto const msg = TxInhibit::parse_datagram (data);
  bool const id_ok = msg.target_id.isEmpty ()
    || msg.target_id == gate->logic_.instance_id ();
  // First lease only. A refresh while already inhibited does not preempt CAT.
  bool const initiating = msg.valid && msg.ttl_ms > 0 && id_ok
    && !gate->inhibited_published_.load (std::memory_order_acquire);
  if (initiating)
    {
      gate->pending_.store (true, std::memory_order_release);
      if (t_rx_ns > 0)
        {
          gate->t_rx_ns_.store (t_rx_ns, std::memory_order_release);
        }
    }
  QMetaObject::invokeMethod (gate, "ingest_shared", Qt::QueuedConnection,
                             Q_ARG (QByteArray, data));
}

TxInhibitGate::~TxInhibitGate ()
{
  // Never emit physicalPtt from the destructor — Hamlib may already be closed.
  shutdown (false);
}

qint64 TxInhibitGate::now_ms () const
{
  // MONOTONIC, deliberately. Hold expiry compares against an absolute value in
  // this same time base, so a wall clock would let a clock *step* corrupt it:
  //
  //   step backwards  -> the hold outlives its timeout by the step size. PTT
  //                      stays off with no packet to explain it.
  //   step forwards   -> the hold ends early. PTT can assert while the priority
  //                      station is still keyed -- the exact failure this
  //                      feature exists to prevent.
  //
  // Not hypothetical for this audience: WSJT-X operators run Meinberg NTP,
  // Dimension4, BktTimeSync and similar, all of which step the system clock,
  // often repeatedly. QElapsedTimer is unaffected by clock changes.
  return uptime_.isValid () ? uptime_.elapsed () : 0;
}

void TxInhibitGate::start_listening ()
{
  stopped_ = false;
  if (!timer_)
    {
      timer_ = new QTimer {this};
      timer_->setInterval (20);
      QObject::connect (timer_, &QTimer::timeout, this, &TxInhibitGate::tick);
      timer_->start ();
    }
  QMutexLocker lock (&gate_mu ());
  current_gate () = this;
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

void TxInhibitGate::shutdown (bool emit_pin)
{
  {
    QMutexLocker lock (&gate_mu ());
    stopped_ = true;
    if (current_gate () == this)
      {
        current_gate () = nullptr;
      }
  }
  pending_.store (false, std::memory_order_release);
  inhibited_published_.store (false, std::memory_order_release);
  intent_ = false;

  if (emit_pin && last_radiate_)
    {
      // Request pin low once while Hamlib is still open (caller responsibility).
      last_radiate_ = false;
      emit_physical_ptt (false);   // teardown must not throw
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
}

void TxInhibitGate::ingest_shared (QByteArray data)
{
  if (stopped_)
    {
      pending_.store (false, std::memory_order_release);
      t_rx_ns_.store (0, std::memory_order_release);
      return;
    }
  bool const was_radiate = last_radiate_;
  qint64 const t_rx = t_rx_ns_.exchange (0, std::memory_order_acq_rel);
  t_pin_ns_.store (0, std::memory_order_release);
  (void) logic_.on_datagram (data, now_ms ());
  apply_line ();
  qint64 const t_pin = t_pin_ns_.exchange (0, std::memory_order_acq_rel);
  bool const dropped = t_rx > 0 && t_pin > t_rx && was_radiate && !last_radiate_;
  emit_state_if_changed (dropped ? t_rx : 0, dropped ? t_pin : 0);
  publish_inhibited ();
  pending_.store (false, std::memory_order_release);
  if (dropped)
    {
      log_pin_release ((t_pin - t_rx) / 1000);
    }
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
  publish_inhibited ();
}

void TxInhibitGate::publish_inhibited ()
{
  inhibited_published_.store (logic_.line_inhibited (now_ms ()),
                              std::memory_order_release);
}

void TxInhibitGate::apply_line ()
{
  if (stopped_)
    {
      return;
    }
  // Sole policy: assert PTT ⇔ want_tx and not hold.
  bool const radiate = intent_ && !logic_.line_inhibited (now_ms ());
  if (radiate == last_radiate_)
    {
      return;
    }
  last_radiate_ = radiate;
  emit_physical_ptt (radiate);
}

// physicalPtt is a DirectConnection into HamlibTransceiver::apply_physical_ptt,
// which calls rig_set_ptt and *throws* on any Hamlib error. This is reached from
// tick() (a QTimer slot) and on_udp_ready() (a readyRead slot), so an escaping
// exception would unwind through QMetaObject::activate into the transceiver
// thread's event loop; ExceptionCatchingApplication::notify catches it and calls
// qFatal, killing the application.
//
// The stock path does not have this exposure: do_ptt is called from
// TransceiverBase::set, which already wraps everything in try/catch. Only the
// gate-driven path is unguarded, so the catch belongs here rather than inside
// apply_physical_ptt -- putting it there would also swallow errors on the stock
// path, where they are supposed to propagate and fail the rig.
//
// Realistic trigger: serial adapter unplugged or radio powered off while a hold
// is active and want_tx is true. The hold expires, the gate tries to assert PTT,
// rig_set_ptt fails, and the application dies.
//
// last_radiate_ is deliberately NOT rolled back on failure. The pin state is
// unknown after a failed set, and rolling back would make the 50 Hz tick retry
// forever, turning one dead cable into an error storm. The rig's own polling
// will notice and fail the transceiver properly.
void TxInhibitGate::emit_physical_ptt (bool radiate)
{
  try
    {
      Q_EMIT physicalPtt (radiate);
    }
  catch (std::exception const& e)
    {
      // Contain the exception (timer/socket slots must not qFatal) but do not
      // hide the failure: pttApplyFailed is wired to Transceiver::failure.
      Q_EMIT pttApplyFailed (QStringLiteral ("TX Inhibit: setting PTT %1 failed: %2")
                             .arg (radiate ? QStringLiteral ("on") : QStringLiteral ("off"))
                             .arg (QString::fromUtf8 (e.what ())));
    }
  catch (...)
    {
      Q_EMIT pttApplyFailed (QStringLiteral ("TX Inhibit: setting PTT %1 failed (unknown error)")
                             .arg (radiate ? QStringLiteral ("on") : QStringLiteral ("off")));
    }
}

void TxInhibitGate::emit_state_if_changed (qint64 t_rx_ns, qint64 t_pin_ns)
{
  qint64 t = now_ms ();
  bool inh = logic_.line_inhibited (t);
  auto badge = logic_.badge_text (t);
  if (inh != last_emitted_inhibited_ || badge != last_badge_)
    {
      last_emitted_inhibited_ = inh;
      last_badge_ = badge;
      Q_EMIT inhibitChanged (inh, badge
                             , logic_.hold_rx (), logic_.release_rx ()
                             , logic_.expiries (), logic_.invalid ()
                             , t_rx_ns, t_pin_ns);
    }
}
