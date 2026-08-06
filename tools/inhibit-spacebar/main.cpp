// inhibit-spacebar — KEY-agent stand-in for local TX Inhibit tests.
//
// Spacebar is a *level* (press = KEY down, release = KEY up + hang).
// Same JSON UDP protocol as docs/TX_INHIBIT.md.
//
// Linux default: keys from *this terminal only* (stdin raw mode) so other
// windows do not false-trigger. Use --global-keys for /dev/input system-wide.
// Windows: GetAsyncKeyState is system-wide; --console-only uses console input.
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
#include <QSocketNotifier>

#include <algorithm>
#include <cmath>
#include <cstdio>

#if defined (Q_OS_WIN)
# include <windows.h>
# include <conio.h>
#elif defined (Q_OS_LINUX)
# include <fcntl.h>
# include <unistd.h>
# include <dirent.h>
# include <linux/input.h>
# include <sys/ioctl.h>
# include <termios.h>
# include <cstring>
#elif defined (Q_OS_UNIX)
# include <unistd.h>
# include <termios.h>
# include <cstring>
#endif

namespace {

static int const kDefaultPort = 22372;
static int const kDefaultTtlMs = 600;
static int const kKeepaliveMs = 200;

// Adaptive hang (optional; KEY agent policy — docs/TX_INHIBIT.md §3.3).
static int const kHangDebounceMs = 80;
static int const kHangMinMs = 200;
static int const kHangMaxMs = 1000;
static double const kHangDitMult = 8.0;
static int const kLongClosureMs = 500;

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

// --- Space level readers ---------------------------------------------------

enum class InputMode
{
  StdinTerminal,  // only keys typed into this terminal (has "focus")
  GlobalKeys      // system-wide keyboard (evdev / GetAsyncKeyState)
};

#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)

#if defined (Q_OS_LINUX)
static int g_evdev_fd = -1;
#endif
static termios g_term_saved {};
static bool g_term_raw = false;
static bool g_stdin_space = false;
static bool g_stdin_quit = false;

void restore_stdin_termios ()
{
  if (g_term_raw)
    {
      tcsetattr (STDIN_FILENO, TCSAFLUSH, &g_term_saved);
      g_term_raw = false;
    }
}

bool setup_stdin_raw ()
{
  if (!isatty (STDIN_FILENO))
    {
      return false;
    }
  if (tcgetattr (STDIN_FILENO, &g_term_saved) != 0)
    {
      return false;
    }
  termios raw = g_term_saved;
  // Input only: non-canonical, no echo, so Space/q/Esc arrive as bytes when
  // this TTY has focus. Do NOT use cfmakeraw() — it clears c_oflag/OPOST so
  // '\n' is not mapped to CR+LF and QTextStream lines fail to wrap/return.
  raw.c_lflag &= static_cast<tcflag_t> (~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG));
  raw.c_iflag &= static_cast<tcflag_t> (~(IXON | IXOFF | ICRNL));
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  // Leave c_oflag alone (keep OPOST / ONLCR) so stdout still wraps correctly.
  if (tcsetattr (STDIN_FILENO, TCSANOW, &raw) != 0)
    {
      return false;
    }
  g_term_raw = true;
  atexit (restore_stdin_termios);
  return true;
}

void poll_stdin_keys ()
{
  if (!g_term_raw)
    {
      return;
    }
  // Drain all pending bytes; track space/q/esc level from press/release is
  // not available via raw tty — we only see press events. Treat as edge:
  // set down flags; level is synthesized in the poll loop via sticky state.
  unsigned char buf[64];
  for (;;)
    {
      ssize_t n = ::read (STDIN_FILENO, buf, sizeof (buf));
      if (n <= 0)
        {
          break;
        }
      for (ssize_t i = 0; i < n; ++i)
        {
          unsigned char c = buf[i];
          if (c == ' ' || c == 0x00)
            {
              // Space press (tty has no release event) — sticky until hang policy
              g_stdin_space = true;
            }
          else if (c == 'q' || c == 'Q' || c == 0x1b || c == 3 /* Ctrl-C */)
            {
              g_stdin_quit = true;
            }
        }
    }
}

// Sticky space for stdin: true after press until clear_stdin_space() after hang.
// For level-style KEY we need press=start and a synthetic up after a short
// "held" window OR require --global-keys for true level. With tty we simulate
// KEY-down while space was recently pressed and user keeps it held by... we
// can't know hold length without release.
//
// Better stdin approach: on press, KEY down; we cannot see release on all
// terminals. Use a simple rule:
//   - space byte → KEY down edge if not already down
//   - after no space byte for debounce while "down", treat as KEY up
// That doesn't work without autorepeat.
//
// Use termios + track last space press time; if space is held, many terminals
// send autorepeat. So:
//   - any space byte → key_down = true, refresh last_space_ms
//   - if key_down && (now - last_space_ms) > 80ms with no new space → KEY up
// Autorepeat typically 30–50ms after delay. 120ms idle → up works for momentary
// and holds while autorepeat continues.

static qint64 g_last_space_byte_ms = -1;

bool space_down_stdin (qint64 now_ms)
{
  poll_stdin_keys ();
  if (g_stdin_space)
    {
      g_stdin_space = false;
      g_last_space_byte_ms = now_ms;
      return true;
    }
  if (g_last_space_byte_ms >= 0 && (now_ms - g_last_space_byte_ms) < 120)
    {
      return true; // held (initial or autorepeat)
    }
  g_last_space_byte_ms = -1;
  return false;
}

bool quit_requested_stdin ()
{
  poll_stdin_keys ();
  return g_stdin_quit;
}

#if defined (Q_OS_LINUX)
bool open_evdev_keyboard ()
{
  if (g_evdev_fd >= 0)
    {
      return true;
    }
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
          unsigned long keybits[(KEY_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
          if (ioctl (fd, EVIOCGBIT (EV_KEY, sizeof (keybits)), keybits) == 0)
            {
              if (!test_bit (KEY_SPACE, keybits))
                {
                  ::close (fd);
                  continue;
                }
            }
          g_evdev_fd = fd;
          closedir (dir);
          return true;
        }
      closedir (dir);
    }
  return false;
}

bool space_down_evdev ()
{
  if (g_evdev_fd < 0 && !open_evdev_keyboard ())
    {
      return false;
    }
  input_event ev {};
  while (read (g_evdev_fd, &ev, sizeof (ev)) == static_cast<ssize_t> (sizeof (ev)))
    {
    }
  unsigned long keybits[(KEY_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
  if (ioctl (g_evdev_fd, EVIOCGKEY (sizeof (keybits)), keybits) < 0)
    {
      return false;
    }
  int const bit = KEY_SPACE;
  return (keybits[bit / (8 * sizeof (long))] >> (bit % (8 * sizeof (long)))) & 1UL;
}

bool quit_requested_evdev ()
{
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
#endif // Q_OS_LINUX

#elif defined (Q_OS_WIN)

static bool g_console_only = false;

bool space_down_global ()
{
  return (GetAsyncKeyState (VK_SPACE) & 0x8000) != 0;
}

bool quit_requested_global ()
{
  return (GetAsyncKeyState ('Q') & 0x8000) != 0
    || (GetAsyncKeyState (VK_ESCAPE) & 0x8000) != 0;
}

// Console focus: only count Space when this console is foreground.
bool space_down_console ()
{
  HWND con = GetConsoleWindow ();
  if (!con || GetForegroundWindow () != con)
    {
      return false;
    }
  return space_down_global ();
}

bool quit_requested_console ()
{
  HWND con = GetConsoleWindow ();
  if (!con || GetForegroundWindow () != con)
    {
      return false;
    }
  return quit_requested_global ();
}

#endif

// --- adaptive hang ---------------------------------------------------------

class HangPolicy
{
public:
  int hang_ms_for_closure (int closure_ms)
  {
    if (closure_ms <= 0)
      {
        return kHangDebounceMs;
      }
    if (closure_ms >= kLongClosureMs)
      {
        return kHangDebounceMs;
      }
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
  QCoreApplication::setApplicationVersion (QStringLiteral ("1.2"));

  QCommandLineParser parser;
  parser.setApplicationDescription (
      QStringLiteral (
          "KEY-agent stand-in: Space level = KEY (press=hold, release=hang then release).\n"
          "Default: only keys for this terminal/console (no false triggers from other windows).\n"
          "Use --global-keys for system-wide Space (old Linux evdev / Win GetAsyncKeyState)."));
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
    QStringLiteral ("Hang after KEY up (default 80). Use 0 for immediate release."),
    QStringLiteral ("ms"),
    QString::number (kHangDebounceMs)};
  QCommandLineOption adaptiveOpt {
    QStringList () << "adaptive",
    QStringLiteral ("CW-style adaptive hang (8×dit). Default is short fixed hang.")};
  QCommandLineOption globalKeysOpt {
    QStringList () << "global-keys",
    QStringLiteral ("Read Space system-wide (other windows can trigger KEY). Not default.")};
  QCommandLineOption quietKaOpt {
    QStringList () << "verbose-keepalive",
    QStringLiteral ("Log every keepalive (default: only first HOLD, hang, RELEASE).")};
  parser.addOption (hostOpt);
  parser.addOption (portOpt);
  parser.addOption (stationOpt);
  parser.addOption (bandOpt);
  parser.addOption (ttlOpt);
  parser.addOption (fixedHangOpt);
  parser.addOption (adaptiveOpt);
  parser.addOption (globalKeysOpt);
  parser.addOption (quietKaOpt);
  parser.process (app);

  QString const host = parser.value (hostOpt);
  quint16 const port = static_cast<quint16> (parser.value (portOpt).toUInt ());
  QString const station = parser.value (stationOpt);
  QString const band = parser.value (bandOpt);
  int const ttl_ms = parser.value (ttlOpt).toInt ();
  bool const adaptive = parser.isSet (adaptiveOpt);
  int const fixed_hang_ms = parser.value (fixedHangOpt).toInt ();
  bool const global_keys = parser.isSet (globalKeysOpt);
  bool const verbose_ka = parser.isSet (quietKaOpt);
  if (ttl_ms < 100 || ttl_ms > 30000)
    {
      QTextStream err (stderr);
      err << "ttl-ms must be 100..30000\n";
      return 2;
    }

  InputMode input_mode = global_keys ? InputMode::GlobalKeys : InputMode::StdinTerminal;

  QString input_note;
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
  if (input_mode == InputMode::StdinTerminal)
    {
      if (setup_stdin_raw ())
        {
          input_note = QStringLiteral (
              "Space only from *this terminal* (raw stdin). "
              "Focus the terminal first. --global-keys for system-wide keyboard.");
        }
      else
        {
          QTextStream err (stderr);
          err << "WARNING: cannot use raw stdin (not a TTY?). ";
#if defined (Q_OS_LINUX)
          err << "Falling back to --global-keys.\n";
          input_mode = InputMode::GlobalKeys;
#else
          err << "No input method available.\n";
#endif
        }
    }
#if defined (Q_OS_LINUX)
  if (input_mode == InputMode::GlobalKeys)
    {
      if (open_evdev_keyboard ())
        {
          input_note = QStringLiteral (
              "Space is SYSTEM-WIDE via /dev/input (any window). "
              "May need group 'input'. Prefer default (no --global-keys) for tests.");
        }
      else
        {
          input_note = QStringLiteral (
              "No readable keyboard under /dev/input — "
              "add user to group 'input', or run in a TTY without --global-keys.");
        }
    }
#endif
#elif defined (Q_OS_WIN)
  if (input_mode == InputMode::StdinTerminal)
    {
      input_note = QStringLiteral (
          "Windows: Space only when *this console* is the foreground window. "
          "Use --global-keys for system-wide (any window).");
    }
  else
    {
      input_note = QStringLiteral (
          "Windows: Space is SYSTEM-WIDE (GetAsyncKeyState). Any window can trigger KEY.");
    }
#else
  input_note = QStringLiteral ("This platform has limited Space support.");
#endif

  QUdpSocket sock;
  qint64 seq = 1;
  HangPolicy hang_policy;
  int holds_sent = 0;
  int keepalives_sent = 0;
  int releases_sent = 0;

  bool key_down = false;
  bool band_held = false;
  qint64 key_down_at_ms = -1;
  qint64 hang_until_ms = -1;

  auto now_ms = [] () { return QDateTime::currentMSecsSinceEpoch (); };

  auto send = [&] (int ttl, bool is_keepalive) {
    QByteArray payload = encode_hold (station, band, seq++, ttl);
    qint64 n = sock.writeDatagram (payload, QHostAddress (host), port);
    QTextStream out (stdout);
    char const * tag = ttl == 0 ? "RELEASE" : (is_keepalive ? "KEEPALIVE" : "HOLD");
    if (ttl == 0)
      {
        ++releases_sent;
      }
    else if (is_keepalive)
      {
        ++keepalives_sent;
      }
    else
      {
        ++holds_sent;
      }
    // Default: log first HOLD, hang notes, RELEASE — not every 200 ms keepalive.
    if (verbose_ka || !is_keepalive || ttl == 0)
      {
        out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
            << "  " << tag
            << "  ttl_ms=" << ttl
            << "  -> " << host << ':' << port
            << "  (holds=" << holds_sent
            << " ka=" << keepalives_sent
            << " rel=" << releases_sent << ")"
            << '\n';
        out.flush ();
      }
    if (n < 0)
      {
        QTextStream err (stderr);
        err << "send failed: " << sock.errorString () << '\n';
      }
  };

  QTextStream out (stdout);
  out << "inhibit-spacebar → " << host << ':' << port << '\n'
      << "  Space     = KEY level (press = hold, release = hang then free)\n"
      << "  q or Esc  = release and quit\n"
      << "  station=" << station << "  band=" << band
      << "  ttl_ms=" << ttl_ms << "  keepalive every " << kKeepaliveMs << " ms (while held)\n"
      << "  hang=" << (adaptive
                       ? QStringLiteral ("adaptive 8×dit (clamp %1–%2 ms)")
                           .arg (kHangMinMs).arg (kHangMaxMs)
                       : QStringLiteral ("fixed %1 ms (use --adaptive for CW hang training)")
                           .arg (std::max (0, fixed_hang_ms)))
      << '\n'
      << "  " << input_note << '\n'
      << "Tip: one short press → 1 HOLD + keepalives only while hang lasts, then 1 RELEASE.\n"
      << "     Default hang is short; old adaptive mode looked like many HOLD lines.\n\n";
  out.flush ();

  QTimer keepalive;
  keepalive.setInterval (kKeepaliveMs);
  QObject::connect (&keepalive, &QTimer::timeout, &app, [&] () {
      if (band_held)
        {
          send (ttl_ms, true);
        }
    });

  QTimer poll;
  poll.setInterval (15);
  QObject::connect (&poll, &QTimer::timeout, &app, [&] () {
      qint64 t = now_ms ();

      bool want_quit = false;
      bool space = false;
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
      if (input_mode == InputMode::StdinTerminal)
        {
          space = space_down_stdin (t);
          want_quit = quit_requested_stdin ();
        }
#if defined (Q_OS_LINUX)
      else
        {
          space = space_down_evdev ();
          want_quit = quit_requested_evdev ();
        }
#endif
#elif defined (Q_OS_WIN)
      if (input_mode == InputMode::StdinTerminal)
        {
          space = space_down_console ();
          want_quit = quit_requested_console ();
        }
      else
        {
          space = space_down_global ();
          want_quit = quit_requested_global ();
        }
#else
      Q_UNUSED (t);
#endif

      if (want_quit)
        {
          if (band_held)
            {
              send (0, false);
              band_held = false;
              keepalive.stop ();
            }
          out << "quit\n";
          out.flush ();
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
          restore_stdin_termios ();
#endif
          QCoreApplication::quit ();
          return;
        }

      // --- KEY down edge
      if (space && !key_down)
        {
          key_down = true;
          key_down_at_ms = t;
          hang_until_ms = -1;
          if (!band_held)
            {
              band_held = true;
              send (ttl_ms, false);
              keepalive.start ();
            }
          out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
              << "  KEY DOWN\n";
          out.flush ();
        }

      // --- KEY up edge → hang (still keepalives until hang ends)
      if (!space && key_down)
        {
          key_down = false;
          int closure = (key_down_at_ms >= 0)
            ? static_cast<int> (t - key_down_at_ms) : 0;
          int hang_ms = adaptive
            ? hang_policy.hang_ms_for_closure (closure)
            : std::max (0, fixed_hang_ms);
          hang_until_ms = t + hang_ms;
          out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
              << "  KEY UP    closure=" << closure << " ms"
              << "  hang=" << hang_ms << " ms";
          if (adaptive && hang_policy.dit_ms () > 0.0)
            {
              out << "  dit≈" << qRound (hang_policy.dit_ms ()) << " ms";
            }
          out << '\n';
          out.flush ();
        }

      // --- hang complete → release
      if (!key_down && hang_until_ms >= 0 && t >= hang_until_ms)
        {
          hang_until_ms = -1;
          if (band_held)
            {
              send (0, false);
              band_held = false;
              keepalive.stop ();
              out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
                  << "  RELEASE (hang done)\n";
              out.flush ();
            }
        }
    });
  poll.start ();

  int rc = app.exec ();
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
  restore_stdin_termios ();
#endif
  return rc;
}
