#ifndef TX_INHIBIT_DROP_HPP__
#define TX_INHIBIT_DROP_HPP__

// Sole writer of the RTS or DTR pin.
//
//   pin = ptt_intent AND NOT inhibit
//
// ptt_intent is stored by the WSJT-X transceiver thread. inhibit is stored
// by the inhibit thread from message type 18. The inhibit thread samples
// both and writes the pin once per wake. rig_set_ptt() is not used.
//
// A separate PTT device is opened when publish_separate() selects it.
// Linux open asserts RTS and DTR, so both lines are cleared once there.
// Later applies write only the selected PTT bit. The other line stays
// released. The port stays open until shutdown_here() or a later
// publish_separate(). CTS is not read. A later KEY source is the agent
// hang output, OR'd with the type 18 hold, not the raw CTS level.
//
// set_inhibit_here() stores t_rx_ns and t_pin_ns when the call drops a pin
// that was high. t_rx_ns is the type 18 socket read. t_pin_ns is the drop
// ioctl return. take_pin_stamps() returns that pair once.

#include <atomic>
#include <cstdint>
#include <mutex>

#include <QObject>
#include <QThread>
#include <QtGlobal>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#endif

namespace TxInhibitDrop
{
  inline bool pin_level (bool intent, bool inhibit)
  {
    return intent && !inhibit;
  }

  // CLOCK_MONOTONIC nanoseconds. Zero when the clock read fails.
  inline qint64 monotonic_ns ()
  {
#if defined(Q_OS_UNIX)
    timespec ts {};
    if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0) return 0;
    return static_cast<qint64> (ts.tv_sec) * 1000000000LL
      + static_cast<qint64> (ts.tv_nsec);
#else
    return 0;
#endif
  }

  inline std::atomic<bool> & ptt_intent ()
  {
    static std::atomic<bool> value {false};
    return value;
  }

  inline std::atomic<bool> & inhibit ()
  {
    static std::atomic<bool> value {false};
    return value;
  }

  inline std::atomic<std::uint64_t> & epoch ()
  {
    static std::atomic<std::uint64_t> value {0};
    return value;
  }

  inline std::atomic<int> & line_fd ()
  {
    static std::atomic<int> value {-1};
    return value;
  }

  inline std::atomic<unsigned> & line_bit ()
  {
    static std::atomic<unsigned> value {0};
    return value;
  }

  inline std::atomic<bool> & own_fd ()
  {
    static std::atomic<bool> value {false};
    return value;
  }

  inline std::atomic<QObject *> & worker ()
  {
    static std::atomic<QObject *> value {nullptr};
    return value;
  }

  // Separate PTT device. The path is written before separate_port() is set.
  inline std::atomic<bool> & separate_port ()
  {
    static std::atomic<bool> value {false};
    return value;
  }

  inline char * separate_path ()
  {
    static char path[512] {};
    return path;
  }

  inline void copy_separate_path (char const * path)
  {
    char * dest = separate_path ();
    std::size_t i = 0;
    if (path)
      {
        for (; i < 511 && path[i] != '\0'; ++i) dest[i] = path[i];
      }
    dest[i] = '\0';
  }

  // One-shot pair for the type 17 that reports this drop.
  // The inhibit thread writes it after the drop ioctl. The status path
  // reads it once. A refresh does not call publish.
  struct PinStampPair
  {
    qint64 t_rx {0};
    qint64 t_pin {0};
  };

  inline PinStampPair & pin_stamps ()
  {
    static PinStampPair value;
    return value;
  }

  inline std::mutex & pin_stamp_mu ()
  {
    static std::mutex mu;
    return mu;
  }

  inline void publish_pin_stamps (qint64 t_rx, qint64 t_pin)
  {
    std::lock_guard<std::mutex> lock {pin_stamp_mu ()};
    pin_stamps ().t_rx = t_rx;
    pin_stamps ().t_pin = t_pin;
  }

  inline void take_pin_stamps (qint64 & t_rx, qint64 & t_pin)
  {
    std::lock_guard<std::mutex> lock {pin_stamp_mu ()};
    t_rx = pin_stamps ().t_rx;
    t_pin = pin_stamps ().t_pin;
    pin_stamps ().t_rx = 0;
    pin_stamps ().t_pin = 0;
    if (!(t_rx > 0 && t_pin > t_rx))
      {
        t_rx = 0;
        t_pin = 0;
      }
  }

  inline void clear_pin_stamps ()
  {
    qint64 t_rx = 0;
    qint64 t_pin = 0;
    take_pin_stamps (t_rx, t_pin);
  }

  // CLOCK_MONOTONIC just after a successful drop ioctl. Zero if no ioctl ran.
  inline qint64 clear_bit_ns (int fd, unsigned bit)
  {
#if defined(Q_OS_UNIX)
    if (fd < 0 || bit == 0) return 0;
#if defined(TIOCMBIC)
    if (ioctl (fd, TIOCMBIC, &bit) < 0) return 0;
#else
    unsigned lines = 0;
    if (ioctl (fd, TIOCMGET, &lines) != 0) return 0;
    lines &= ~bit;
    if (ioctl (fd, TIOCMSET, &lines) != 0) return 0;
#endif
    return monotonic_ns ();
#else
    Q_UNUSED (fd);
    Q_UNUSED (bit);
    return 0;
#endif
  }

  inline void raise_pin ()
  {
#if defined(Q_OS_UNIX)
    int const fd = line_fd ().load (std::memory_order_acquire);
    unsigned bit = line_bit ().load (std::memory_order_acquire);
    if (fd < 0 || bit == 0) return;
#if defined(TIOCMBIS)
    ioctl (fd, TIOCMBIS, &bit);
#else
    unsigned lines = 0;
    if (ioctl (fd, TIOCMGET, &lines) != 0) return;
    lines |= bit;
    ioctl (fd, TIOCMSET, &lines);
#endif
#endif
  }

  // Inhibit thread only. The separate port is already open. This writes
  // only the PTT bit. Returns the drop time, or zero when the pin was
  // raised or no ioctl ran.
  inline qint64 apply_separate (bool high)
  {
#if defined(Q_OS_UNIX)
    unsigned const bit = line_bit ().load (std::memory_order_acquire);
    int const fd = line_fd ().load (std::memory_order_acquire);
    if (bit == 0 || fd < 0) return 0;
    if (high)
      {
        raise_pin ();
        return 0;
      }
    return clear_bit_ns (fd, bit);
#else
    Q_UNUSED (high);
    return 0;
#endif
  }

  // Inhibit thread only. Returns the drop-ioctl time, or zero when the
  // pin was raised or no ioctl ran.
  inline qint64 apply ()
  {
    bool const high = pin_level (ptt_intent ().load (std::memory_order_acquire),
                                 inhibit ().load (std::memory_order_acquire));
    if (separate_port ().load (std::memory_order_acquire))
      {
        return apply_separate (high);
      }
    if (high)
      {
        raise_pin ();
        return 0;
      }
    int const fd = line_fd ().load (std::memory_order_acquire);
    unsigned const bit = line_bit ().load (std::memory_order_acquire);
    return clear_bit_ns (fd, bit);
  }

  inline void wake ()
  {
    QObject * w = worker ().load (std::memory_order_acquire);
    if (!w) return;
    QMetaObject::invokeMethod (w, "apply_pin", Qt::QueuedConnection);
  }

  inline void set_ptt_intent (bool on)
  {
    ptt_intent ().store (on, std::memory_order_release);
    wake ();
  }

  // Inhibit thread only. Bumps the epoch so a stale lease-expiry cannot
  // clear an inhibit that started after the expiry was queued.
  // t_rx_ns is CLOCK_MONOTONIC at the type 18 socket read. Zero skips the
  // pin log. A stamp is stored only when this call drops a pin that was high.
  inline void set_inhibit_here (bool active, qint64 t_rx_ns = 0)
  {
    bool const was_high = pin_level (ptt_intent ().load (std::memory_order_acquire),
                                     inhibit ().load (std::memory_order_acquire));
    epoch ().fetch_add (1, std::memory_order_acq_rel);
    inhibit ().store (active, std::memory_order_release);
    qint64 const t_pin = apply ();
    bool const now_high = pin_level (ptt_intent ().load (std::memory_order_acquire),
                                     inhibit ().load (std::memory_order_acquire));
    if (t_rx_ns > 0 && t_pin > t_rx_ns && was_high && !now_high)
      {
        publish_pin_stamps (t_rx_ns, t_pin);
      }
  }

  inline void release_if_epoch (quint64 observed)
  {
    if (epoch ().load (std::memory_order_acquire) != observed) return;
    if (!inhibit ().load (std::memory_order_acquire)) return;
    inhibit ().store (false, std::memory_order_release);
    apply ();
  }

  inline void request_release ()
  {
    QObject * w = worker ().load (std::memory_order_acquire);
    if (!w) return;
    quint64 const observed = epoch ().load (std::memory_order_acquire);
    QMetaObject::invokeMethod (w, "release_if_epoch", Qt::QueuedConnection,
                               Q_ARG (quint64, observed));
  }

  inline void attach (QObject * w)
  {
    worker ().store (w, std::memory_order_release);
  }

  inline void publish (int fd, unsigned bit, bool owned)
  {
    separate_port ().store (false, std::memory_order_release);
    line_bit ().store (bit, std::memory_order_release);
    int const old = line_fd ().exchange (fd, std::memory_order_acq_rel);
    bool const was_owned = own_fd ().exchange (owned, std::memory_order_acq_rel);
#if defined(Q_OS_UNIX)
    if (was_owned && old >= 0 && old != fd) ::close (old);
#else
    Q_UNUSED (old);
    Q_UNUSED (was_owned);
#endif
    wake ();
  }

  // Separate PTT device. Open now, and hold until shutdown or a later publish.
  // Linux open asserts both modem lines. Clear them once here. Later applies
  // write only `bit`.
  inline void publish_separate (char const * path, unsigned bit)
  {
    copy_separate_path (path);
    line_bit ().store (bit, std::memory_order_release);
    int const old = line_fd ().exchange (-1, std::memory_order_acq_rel);
    bool const was_owned = own_fd ().exchange (true, std::memory_order_acq_rel);
#if defined(Q_OS_UNIX)
    if (was_owned && old >= 0) ::close (old);
    int fd = -1;
    if (path != nullptr && path[0] != '\0')
      {
        fd = ::open (path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
      }
    if (fd >= 0)
      {
        unsigned const both = TIOCM_RTS | TIOCM_DTR;
        ioctl (fd, TIOCMBIC, &both);
        line_fd ().store (fd, std::memory_order_release);
      }
#else
    Q_UNUSED (old);
    Q_UNUSED (was_owned);
#endif
    separate_port ().store (true, std::memory_order_release);
    wake ();
  }

  // Inhibit thread, or the transceiver thread when the worker is already gone.
  inline void shutdown_here ()
  {
    // A release already queued must not reopen the port after this.
    epoch ().fetch_add (1, std::memory_order_acq_rel);
    ptt_intent ().store (false, std::memory_order_release);
    inhibit ().store (false, std::memory_order_release);
    apply ();
    clear_pin_stamps ();
    int const fd = line_fd ().exchange (-1, std::memory_order_acq_rel);
    bool const owned = own_fd ().exchange (false, std::memory_order_acq_rel);
    separate_port ().store (false, std::memory_order_release);
    line_bit ().store (0, std::memory_order_release);
#if defined(Q_OS_UNIX)
    if (owned && fd >= 0) ::close (fd);
#else
    Q_UNUSED (fd);
    Q_UNUSED (owned);
#endif
  }

  inline void shutdown ()
  {
    QObject * w = worker ().load (std::memory_order_acquire);
    if (w && QThread::currentThread () != w->thread ())
      {
        QMetaObject::invokeMethod (w, "shutdown_pin", Qt::BlockingQueuedConnection);
        return;
      }
    shutdown_here ();
  }
}

#endif
