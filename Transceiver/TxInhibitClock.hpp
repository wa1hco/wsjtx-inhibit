#ifndef TX_INHIBIT_CLOCK_HPP__
#define TX_INHIBIT_CLOCK_HPP__

// Shared CLOCK_MONOTONIC stamps for one inhibit unkey.
// t_rx is the type 18 socket read. t_pin is the RTS/DTR ioctl.
// The type 17 sender takes the pair once; later repeats send zeros.

#include <atomic>
#include <ctime>

#include <QtGlobal>

namespace TxInhibitClock
{
  inline qint64 monotonic_ns ()
  {
    timespec ts {};
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return static_cast<qint64> (ts.tv_sec) * 1000000000LL
      + static_cast<qint64> (ts.tv_nsec);
  }

  inline std::atomic<qint64> & rx_ns ()
  {
    static std::atomic<qint64> value {0};
    return value;
  }

  inline std::atomic<qint64> & pin_ns ()
  {
    static std::atomic<qint64> value {0};
    return value;
  }

  // Set by TxInhibitTransceiver around the wrapped set() that drops PTT.
  inline std::atomic<bool> & arm_pin ()
  {
    static std::atomic<bool> value {false};
    return value;
  }

  inline void note_rx ()
  {
    rx_ns ().store (monotonic_ns (), std::memory_order_release);
  }

  // Keep the earliest stamp. A direct ioctl may land before do_ptt runs.
  inline void note_pin (qint64 ns)
  {
    if (ns <= 0) return;
    qint64 expected = 0;
    pin_ns ().compare_exchange_strong (expected, ns, std::memory_order_release);
  }

  // Returns 0 when the modem line was cleared. Installed by HamlibTransceiver
  // as ser_set_rts or ser_set_dtr, which do not take the rig lock.
  using LineDrop = int (*) (void * port);

  inline std::atomic<LineDrop> & line_drop ()
  {
    static std::atomic<LineDrop> value {nullptr};
    return value;
  }

  inline std::atomic<void *> & line_port ()
  {
    static std::atomic<void *> value {nullptr};
    return value;
  }

  inline void clear_ptt ()
  {
    line_port ().store (nullptr, std::memory_order_release);
    line_drop ().store (nullptr, std::memory_order_release);
  }

  // drop is stored before the port, so a non-null port is safe to call.
  inline void publish_ptt (LineDrop drop, void * port)
  {
    if (!drop || !port)
      {
        clear_ptt ();
        return;
      }
    line_drop ().store (drop, std::memory_order_release);
    line_port ().store (port, std::memory_order_release);
  }

  // Clear RTS or DTR from the thread that read the datagram.
  inline qint64 drop_direct ()
  {
    void * port = line_port ().load (std::memory_order_acquire);
    LineDrop const drop = line_drop ().load (std::memory_order_acquire);
    if (!port || !drop) return 0;
    if (drop (port) != 0) return 0;
    qint64 const t = monotonic_ns ();
    note_pin (t);
    return t;
  }

  // Consume a completed pair. Zeros if this status is not that unkey.
  inline void take (qint64 & rx, qint64 & pin)
  {
    rx = rx_ns ().exchange (0, std::memory_order_acq_rel);
    pin = pin_ns ().exchange (0, std::memory_order_acq_rel);
    if (!(rx > 0 && pin > rx))
      {
        rx = 0;
        pin = 0;
      }
  }
}

#endif
