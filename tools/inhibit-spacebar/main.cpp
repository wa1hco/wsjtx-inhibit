// inhibit-spacebar — KEY-agent stand-in for local TX Inhibit tests.
//
// Ships next to wsjtx.exe (bin/). Spacebar is a *level* (press = KEY down,
// release = KEY up), so you can key CW-style and experiment with hang timing.
// Same JSON UDP protocol as docs/TX_INHIBIT.md.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>
#include <QByteArray>
#include <QString>
#include <QTextStream>
#include <QDateTime>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>

#if defined (Q_OS_WIN)
# include <windows.h>
#elif defined (Q_OS_LINUX)
# include <fcntl.h>
# include <unistd.h>
# include <dirent.h>
# include <linux/input.h>
# include <sys/ioctl.h>
# include <cstring>
#endif

namespace {

static int const kDefaultPort = 22372;
static int const kDefaultTtlMs = 600;
static int const kKeepaliveMs = 200;

// Adaptive hang (KEY agent policy — see docs/TX_INHIBIT.md §3.3):
// short CW-like closures estimate a dit; hang ≈ 8×dit, clamped.
// long closures (SSB-style PTT) use a short debounce hang only.
static int const kHangDebounceMs = 80;
static int const kHangMinMs = 200;
static int const kHangMaxMs = 1000;
static double const kHangDitMult = 8.0;
static int const kLongClosureMs = 500; // above this → treat as "long" key

QByteArray encode_hold (QString const& station, QString const& band,
                        qint64 seq, int ttl_ms)
{
  QByteArray body;
  body += "{\"tx_inhibit\":1,\"ttl_ms\":";
  body += QByteArray::number (ttl_ms);
  body += ",\"station\":\"";
  body += station.toUtf8 ();
  body += "\",\"band\":\"";
  body += band.toUtf8 ();
  body += "\",\"seq\":";
  body += QByteArray::number (seq);
  body += '}';
  return body;
}

// --- platform: Spacebar level (down = true) --------------------------------

#if defined (Q_OS_WIN)

bool space_down ()
{
  return (GetAsyncKeyState (VK_SPACE) & 0x8000) != 0;
}

bool quit_requested ()
{
  // q / Esc while focused anywhere is awkward with GetAsyncKeyState; poll both.
  return (GetAsyncKeyState ('Q') & 0x8000) != 0
    || (GetAsyncKeyState (VK_ESCAPE) & 0x8000) != 0;
}

QString input_mode_note ()
{
  return QStringLiteral ("Windows: Space level via GetAsyncKeyState (focus any window)");
}

#elif defined (Q_OS_LINUX)

// Prefer a keyboard event node and EVIOCGKEY for true level (press/release).
static int g_evdev_fd = -1;

bool open_evdev_keyboard ()
{
  if (g_evdev_fd >= 0)
    {
      return true;
    }
  // Prefer by-path *-event-kbd, then scan event*
  char const * dirs[] = {"/dev/input/by-path", "/dev/input", nullptr};
  for (int d = 0; dirs[d]; ++d)
    {
      DIR * dir = opendir (dirs[d]);
      if (!dir)
        {
          continue;
        }
      while (dirent * ent = readdir (dir))
        {
          QString name = QString::fromLocal8Bit (ent->d_name);
          if (name == QLatin1String (".") || name == QLatin1String (".."))
            {
              continue;
            }
          if (d == 0 && !name.contains (QLatin1String ("event-kbd")))
            {
              continue;
            }
          if (d == 1 && !name.startsWith (QLatin1String ("event")))
            {
              continue;
            }
          QByteArray path = QByteArray (dirs[d]) + '/' + QByteArray (ent->d_name);
          int fd = ::open (path.constData (), O_RDONLY | O_NONBLOCK);
          if (fd < 0)
            {
              continue;
            }
          // Must support KEY_SPACE in EV_KEY
          unsigned long evbits[(EV_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
          if (ioctl (fd, EVIOCGBIT (0, sizeof (evbits)), evbits) < 0)
            {
              ::close (fd);
              continue;
            }
          auto test_bit = [] (int bit, unsigned long const * arr) {
            return (arr[bit / (8 * sizeof (long))] >> (bit % (8 * sizeof (long)))) & 1UL;
          };
          if (!test_bit (EV_KEY, evbits))
            {
              ::close (fd);
              continue;
            }
          g_evdev_fd = fd;
          closedir (dir);
          return true;
        }
      closedir (dir);
    }
  return false;
}

bool space_down ()
{
  if (g_evdev_fd < 0 && !open_evdev_keyboard ())
    {
      return false;
    }
  // Drain events (keeps driver happy) then sample key state bitmap
  input_event ev {};
  while (read (g_evdev_fd, &ev, sizeof (ev)) == static_cast<ssize_t> (sizeof (ev)))
    {
    }
  // KEY_MAX is large; KEY_SPACE is 57
  unsigned long keybits[(KEY_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
  if (ioctl (g_evdev_fd, EVIOCGKEY (sizeof (keybits)), keybits) < 0)
    {
      return false;
    }
  int const bit = KEY_SPACE;
  return (keybits[bit / (8 * sizeof (long))] >> (bit % (8 * sizeof (long)))) & 1UL;
}

bool quit_requested ()
{
  // Ctrl-C still works via signal; also check Q key on same keyboard state
  if (g_evdev_fd < 0)
    {
      return false;
    }
  unsigned long keybits[(KEY_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
  if (ioctl (g_evdev_fd, EVIOCGKEY (sizeof (keybits)), keybits) < 0)
    {
      return false;
    }
  auto down = [&] (int code) {
    return (keybits[code / (8 * sizeof (long))] >> (code % (8 * sizeof (long)))) & 1UL;
  };
  return down (KEY_Q) || down (KEY_ESC);
}

QString input_mode_note ()
{
  if (g_evdev_fd >= 0 || open_evdev_keyboard ())
    {
      return QStringLiteral ("Linux: Space level via /dev/input (EVIOCGKEY). "
                             "May need membership in group 'input'.");
    }
  return QStringLiteral ("Linux: no readable keyboard under /dev/input — "
                         "add user to group 'input' or run with access to event devices.");
}

#else

// macOS / other: best-effort false (no level API wired yet)
bool space_down () { return false; }
bool quit_requested () { return false; }
QString input_mode_note ()
{
  return QStringLiteral ("This platform has no Space level reader yet; use tools/send_inhibit_hold.py.");
}

#endif

// --- adaptive hang ---------------------------------------------------------

class HangPolicy
{
public:
  // On KEY open after a closure of length closure_ms, return hang duration.
  int hang_ms_for_closure (int closure_ms)
  {
    if (closure_ms <= 0)
      {
        return kHangDebounceMs;
      }
    // Long key-down (SSB PTT style): short debounce only.
    if (closure_ms >= kLongClosureMs)
      {
        return kHangDebounceMs;
      }
    // Short element: update dit estimate (EMA) and hang = 8×dit.
    double sample = static_cast<double> (closure_ms);
    if (dit_ms_ <= 0.0)
      {
        dit_ms_ = sample;
      }
    else
      {
        dit_ms_ = 0.35 * sample + 0.65 * dit_ms_;
      }
    int hang = static_cast<int> (std::lround (kHangDitMult * dit_ms_));
    hang = std::max (kHangMinMs, std::min (kHangMaxMs, hang));
    return hang;
  }

  double dit_ms () const { return dit_ms_; }

private:
  double dit_ms_ {0.0};
};

} // namespace

int main (int argc, char * argv[])
{
  QCoreApplication app (argc, argv);
  QCoreApplication::setApplicationName (QStringLiteral ("inhibit-spacebar"));
  QCoreApplication::setApplicationVersion (QStringLiteral ("1.1"));

  QCommandLineParser parser;
  parser.setApplicationDescription (
      QStringLiteral (
          "KEY-agent stand-in: Spacebar level = KEY (press=hold, release=hang then release).\n"
          "Key CW on Space to exercise adaptive hang. Default target 127.0.0.1:22372.\n"
          "Installs next to wsjtx.exe in bin/."));
  parser.addHelpOption ();
  parser.addVersionOption ();
  QCommandLineOption hostOpt {QStringList () << "H" << "host",
                              QStringLiteral ("Seat host (default 127.0.0.1)"),
                              QStringLiteral ("host"),
                              QStringLiteral ("127.0.0.1")};
  QCommandLineOption portOpt {QStringList () << "p" << "port",
                              QStringLiteral ("Inhibit UDP port (default 22372)"),
                              QStringLiteral ("port"),
                              QString::number (kDefaultPort)};
  QCommandLineOption stationOpt {QStringList () << "s" << "station",
                                 QStringLiteral ("Badge station id"),
                                 QStringLiteral ("name"),
                                 QStringLiteral ("TEST-SSB")};
  QCommandLineOption bandOpt {QStringList () << "b" << "band",
                              QStringLiteral ("Band field (informational)"),
                              QStringLiteral ("band"),
                              QStringLiteral ("144")};
  QCommandLineOption ttlOpt {QStringList () << "t" << "ttl-ms",
                             QStringLiteral ("Hold TTL ms for keepalives (default 600)"),
                             QStringLiteral ("ms"),
                             QString::number (kDefaultTtlMs)};
  QCommandLineOption fixedHangOpt {
    QStringList () << "fixed-hang-ms",
    QStringLiteral ("Disable adaptive hang; always wait this many ms after KEY up (0=debounce only)"),
    QStringLiteral ("ms")};
  parser.addOption (hostOpt);
  parser.addOption (portOpt);
  parser.addOption (stationOpt);
  parser.addOption (bandOpt);
  parser.addOption (ttlOpt);
  parser.addOption (fixedHangOpt);
  parser.process (app);

  QString const host = parser.value (hostOpt);
  quint16 const port = static_cast<quint16> (parser.value (portOpt).toUInt ());
  QString const station = parser.value (stationOpt);
  QString const band = parser.value (bandOpt);
  int const ttl_ms = parser.value (ttlOpt).toInt ();
  bool const use_fixed_hang = parser.isSet (fixedHangOpt);
  int const fixed_hang_ms = use_fixed_hang ? parser.value (fixedHangOpt).toInt () : 0;
  if (ttl_ms < 100 || ttl_ms > 30000)
    {
      QTextStream err (stderr);
      err << "ttl-ms must be 100..30000\n";
      return 2;
    }

  QUdpSocket sock;
  qint64 seq = 1;
  HangPolicy hang_policy;

  // KEY state machine
  bool key_down = false;           // raw Space level
  bool band_held = false;          // sending holds (KEY down or in hang)
  qint64 key_down_at_ms = -1;
  qint64 hang_until_ms = -1;       // if >=0, in hang until this wall time

  auto now_ms = [] () { return QDateTime::currentMSecsSinceEpoch (); };

  auto send = [&] (int ttl) {
    QByteArray payload = encode_hold (station, band, seq++, ttl);
    qint64 n = sock.writeDatagram (payload, QHostAddress (host), port);
    QTextStream out (stdout);
    out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
        << (ttl ? "  HOLD" : "  RELEASE")
        << "  ttl_ms=" << ttl
        << "  -> " << host << ':' << port
        << '\n';
    out.flush ();
    if (n < 0)
      {
        QTextStream err (stderr);
        err << "send failed: " << sock.errorString () << '\n';
      }
  };

  QTextStream out (stdout);
  out << "inhibit-spacebar → " << host << ':' << port << '\n'
      << "  Spacebar  = KEY level (press = hold, release = hang then free)\n"
      << "  Key CW on Space to train adaptive hang (short elements → longer hang)\n"
      << "  q or Esc  = release and quit\n"
      << "  station=" << station << "  band=" << band
      << "  ttl_ms=" << ttl_ms << "  keepalive=" << kKeepaliveMs << " ms\n"
      << "  hang=" << (use_fixed_hang
                       ? QStringLiteral ("fixed %1 ms").arg (fixed_hang_ms)
                       : QStringLiteral ("adaptive (8×dit, clamp %1–%2 ms; long key → %3 ms debounce)")
                           .arg (kHangMinMs).arg (kHangMaxMs).arg (kHangDebounceMs))
      << '\n'
      << "  " << input_mode_note () << '\n'
      << "Run wsjtx-inhibit with PTT=RTS/DTR, then hold Space here.\n\n";
  out.flush ();

#if defined (Q_OS_LINUX)
  if (!open_evdev_keyboard ())
    {
      QTextStream err (stderr);
      err << "WARNING: " << input_mode_note () << '\n';
    }
#endif

  QTimer keepalive;
  keepalive.setInterval (kKeepaliveMs);
  QObject::connect (&keepalive, &QTimer::timeout, &app, [&] () {
      if (band_held)
        {
          send (ttl_ms);
        }
    });

  QTimer poll;
  poll.setInterval (10); // 100 Hz — enough for CW elements and hang edges
  QObject::connect (&poll, &QTimer::timeout, &app, [&] () {
      qint64 t = now_ms ();

      if (quit_requested ())
        {
          if (band_held)
            {
              send (0);
              band_held = false;
              keepalive.stop ();
            }
          out << "quit\n";
          out.flush ();
          QCoreApplication::quit ();
          return;
        }

      bool space = space_down ();

      // --- KEY down edge
      if (space && !key_down)
        {
          key_down = true;
          key_down_at_ms = t;
          hang_until_ms = -1; // cancel hang
          if (!band_held)
            {
              band_held = true;
              send (ttl_ms);
              keepalive.start ();
            }
          out << t << "  KEY DOWN\n";
          out.flush ();
        }

      // --- KEY up edge → start hang (still held for hang_ms)
      if (!space && key_down)
        {
          key_down = false;
          int closure = (key_down_at_ms >= 0)
            ? static_cast<int> (t - key_down_at_ms) : 0;
          int hang_ms = use_fixed_hang
            ? std::max (0, fixed_hang_ms)
            : hang_policy.hang_ms_for_closure (closure);
          hang_until_ms = t + hang_ms;
          out << t << "  KEY UP    closure=" << closure << " ms"
              << "  hang=" << hang_ms << " ms";
          if (!use_fixed_hang && hang_policy.dit_ms () > 0.0)
            {
              out << "  dit≈" << qRound (hang_policy.dit_ms ()) << " ms";
            }
          out << '\n';
          out.flush ();
        }

      // --- hang complete → release band
      if (!key_down && hang_until_ms >= 0 && t >= hang_until_ms)
        {
          hang_until_ms = -1;
          if (band_held)
            {
              send (0);
              band_held = false;
              keepalive.stop ();
              out << t << "  RELEASE (hang done)\n";
              out.flush ();
            }
        }
    });
  poll.start ();

  return app.exec ();
}
