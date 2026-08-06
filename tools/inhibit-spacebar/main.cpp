// inhibit-spacebar — KEY-agent stand-in for TX Inhibit (docs/TX_INHIBIT.md §3)
//
// Canonical name: inhibit-spacebar (installed as bin/inhibit-spacebar next to
// wsjtx). Implements two cooperating roles on one rare KEY line:
//
//   KEY stand-in key: left quote / grave accent `  (not Space — typing
//   spaces must not false-trigger holds).
//
//   Hold sender   — hold immediately (ttl_ms = hold_timeout_ms), keepalives
//                   ~200 ms, release with ttl_ms=0 (cancel keepalives first).
//   KEYing monitor — classify break-in CW vs continuous KEY (non-break-in /
//                   SSB); measure dit; hang = 1.5× word gap (break-in only);
//                   EOT signals Hold sender to release.
//
// KEY level: press = assert, release = open (gap or EOT).
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
// hold_timeout_ms (wire ttl_ms) — safety on lost hold packets, not hang.
static int const kDefaultHoldTimeoutMs = 600;
static int const kKeepaliveMs = 200;

// Hang = 1.5 × word gap; word gap = 7 dits → hang = 10.5 × dit (Paris).
// WPM 10..40 ⇒ dit 120..30 ms ⇒ hang 1260..315 ms (docs/TX_INHIBIT.md §3.4).
static double const kHangWordGapMult = 1.5;
static int const kWordGapDits = 7;
static int const kHangMinMs = 315;   // ~40 WPM
static int const kHangMaxMs = 1260;  // ~10 WPM
// Continuous KEY (SSB / non-break-in): no element gaps → hang 0.
// Marks longer than this without short gaps are treated as continuous.
static int const kContinuousMarkMs = 500;
// Element gap upper bound while still "in character/word" (before hang EOT).
// Letter gap = 3 dits; use up to ~5 dits as "still break-in" open.
static double const kMaxIntraTxGapDits = 5.0;

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

// --- KEY level readers (grave/backtick ` — rare key, not Space) ------------
// Linux: KEY_GRAVE. Windows: VK_OEM_3 (US `~ key). Stdin: '`' (or '~').

enum class InputMode
{
  StdinTerminal,  // only keys typed into this terminal (has "focus")
  GlobalKeys      // system-wide keyboard (evdev / GetAsyncKeyState)
};

static bool is_key_char (unsigned char c)
{
  return c == '`' || c == '~'; // grave / shifted grave on many layouts
}

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
  // Input only: non-canonical, no echo, so `/q/Esc arrive as bytes when
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
          if (is_key_char (c))
            {
              // Grave press (tty has no release event) — sticky hold synthesised
              g_stdin_space = true;
            }
          else if (c == 'q' || c == 'Q' || c == 0x1b || c == 3 /* Ctrl-C */)
            {
              g_stdin_quit = true;
            }
        }
    }
}

// Degraded stdin KEY (only if /dev/input unavailable): TTY has no release.
// Use a *short* sticky window (~2 polls) so short taps are not forced to 120ms.
// Long holds need autorepeat (or use EVIOCGKEY). Prefer open_evdev_keyboard().
static int const kStdinStickyMs = 25;
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
  if (g_last_space_byte_ms >= 0 && (now_ms - g_last_space_byte_ms) < kStdinStickyMs)
    {
      return true;
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
              // Prefer keyboard that has grave/backtick (KEY_GRAVE).
              if (!test_bit (KEY_GRAVE, keybits))
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

// True KEY *level* via kernel key state (no 120ms sticky). Call often.
bool space_down_evdev ()
{
  if (g_evdev_fd < 0 && !open_evdev_keyboard ())
    {
      return false;
    }
  // Drain queue so EVIOCGKEY reflects current state
  input_event ev {};
  while (read (g_evdev_fd, &ev, sizeof (ev)) == static_cast<ssize_t> (sizeof (ev)))
    {
    }
  unsigned long keybits[(KEY_MAX + 8 * sizeof (long) - 1) / (8 * sizeof (long))] = {};
  if (ioctl (g_evdev_fd, EVIOCGKEY (sizeof (keybits)), keybits) < 0)
    {
      return false;
    }
  int const bit = KEY_GRAVE; // left quote / backtick `
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
  // US/PC keyboard: VK_OEM_3 is the `~ key (left of 1). Rare in normal typing.
  return (GetAsyncKeyState (VK_OEM_3) & 0x8000) != 0;
}

bool quit_requested_global ()
{
  return (GetAsyncKeyState ('Q') & 0x8000) != 0
    || (GetAsyncKeyState (VK_ESCAPE) & 0x8000) != 0;
}

// Console focus: only count KEY when this console is foreground.
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

// --- KEYing monitor (docs/TX_INHIBIT.md §3.2–3.5) ---------------------------

enum class KeyClass
{
  Unknown,      // not yet classified this transmission
  BreakInCw,    // saw short KEY opens (element/letter gaps)
  Continuous    // long mark, no short gaps → non-break-in CW / SSB
};

// Tracks KEY edges; decides hang (ms) when KEY opens (may be gap or EOT).
class KeyingMonitor
{
public:
  void reset_transmission ()
  {
    key_class_ = KeyClass::Unknown;
    dit_ms_ = 0.0;
    mark_open_ = false;
    mark_start_ms_ = -1;
    gap_start_ms_ = -1;
  }

  // KEY assert edge
  void on_key_assert (qint64 now_ms)
  {
    if (gap_start_ms_ >= 0)
      {
        int gap = static_cast<int> (now_ms - gap_start_ms_);
        on_gap_closed (gap);
        gap_start_ms_ = -1;
      }
    mark_open_ = true;
    mark_start_ms_ = now_ms;
  }

  // KEY open edge — returns hang_ms to wait before EOT (0 = release now).
  // If KEY asserts again before hang expires, call on_key_assert (not EOT).
  int on_key_open (qint64 now_ms)
  {
    int mark_ms = 0;
    if (mark_open_ && mark_start_ms_ >= 0)
      {
        mark_ms = static_cast<int> (now_ms - mark_start_ms_);
        note_mark (mark_ms);
      }
    mark_open_ = false;
    mark_start_ms_ = -1;
    gap_start_ms_ = now_ms;

    if (key_class_ == KeyClass::Continuous
        || (key_class_ == KeyClass::Unknown && mark_ms >= kContinuousMarkMs))
      {
        key_class_ = KeyClass::Continuous;
        return 0; // hang 0 — non-break-in / SSB
      }
    if (key_class_ == KeyClass::BreakInCw || dit_ms_ > 0.0)
      {
        key_class_ = KeyClass::BreakInCw;
        return hang_ms_break_in ();
      }
    // Unknown, short mark, no gaps yet: wait a short provisional hang based
    // on this mark as a dit candidate; if KEY returns, on_gap_closed fires.
    if (mark_ms > 0 && mark_ms < kContinuousMarkMs)
      {
        note_dit_sample (static_cast<double> (mark_ms));
        return hang_ms_break_in ();
      }
    return 0;
  }

  KeyClass key_class () const { return key_class_; }
  double dit_ms () const { return dit_ms_; }

  int hang_ms_break_in () const
  {
    if (dit_ms_ <= 0.0)
      {
        return kHangMinMs;
      }
    // hang = 1.5 × word_gap = 1.5 × 7 × dit = 10.5 × dit
    double hang = kHangWordGapMult * static_cast<double> (kWordGapDits) * dit_ms_;
    int h = static_cast<int> (std::lround (hang));
    return std::max (kHangMinMs, std::min (kHangMaxMs, h));
  }

  char const * class_name () const
  {
    switch (key_class_)
      {
      case KeyClass::BreakInCw: return "break-in CW";
      case KeyClass::Continuous: return "continuous (non-break-in/SSB)";
      default: return "unknown";
      }
  }

private:
  void note_mark (int mark_ms)
  {
    if (mark_ms <= 0)
      {
        return;
      }
    if (mark_ms >= kContinuousMarkMs && key_class_ != KeyClass::BreakInCw)
      {
        key_class_ = KeyClass::Continuous;
        return;
      }
    // Dit-like: shorter than ~2× current dit, or first short mark
    if (dit_ms_ <= 0.0)
      {
        if (mark_ms < kContinuousMarkMs)
          {
            note_dit_sample (static_cast<double> (mark_ms));
          }
        return;
      }
    if (mark_ms < 2.0 * dit_ms_)
      {
        note_dit_sample (static_cast<double> (mark_ms));
      }
    // else dah / long mark — speed from dits only
  }

  void note_dit_sample (double sample_ms)
  {
    if (sample_ms <= 0.0)
      {
        return;
      }
    if (dit_ms_ <= 0.0)
      {
        dit_ms_ = sample_ms;
      }
    else
      {
        dit_ms_ = 0.35 * sample_ms + 0.65 * dit_ms_;
      }
  }

  void on_gap_closed (int gap_ms)
  {
    if (gap_ms <= 0)
      {
        return;
      }
    // Short open between marks ⇒ break-in CW (element or letter gap).
    double max_gap = (dit_ms_ > 0.0)
      ? kMaxIntraTxGapDits * dit_ms_
      : static_cast<double> (kContinuousMarkMs);
    if (gap_ms <= max_gap)
      {
        key_class_ = KeyClass::BreakInCw;
      }
  }

  KeyClass key_class_ {KeyClass::Unknown};
  double dit_ms_ {0.0};
  bool mark_open_ {false};
  qint64 mark_start_ms_ {-1};
  qint64 gap_start_ms_ {-1};
};

} // namespace

int main (int argc, char * argv[])
{
  QCoreApplication app (argc, argv);
  QCoreApplication::setApplicationName (QStringLiteral ("inhibit-spacebar"));
  QCoreApplication::setApplicationVersion (QStringLiteral ("2.0"));

  QCommandLineParser parser;
  parser.setApplicationDescription (
      QStringLiteral (
          "KEY-agent stand-in (docs/TX_INHIBIT.md §3): Hold sender + KEYing monitor.\n"
          "KEY key = left quote / grave ` (not Space — typing won't false-trigger).\n"
          "Break-in CW: hang = 1.5×word gap; continuous KEY: hang 0.\n"
          "Default: only this terminal/console. Use --global-keys for system-wide."));
  parser.addHelpOption ();
  parser.addVersionOption ();
  QCommandLineOption hostOpt {QStringList () << "H" << "host",
                              QStringLiteral ("WSJT-X station host (default 127.0.0.1)"),
                              QStringLiteral ("host"),
                              QStringLiteral ("127.0.0.1")};
  QCommandLineOption portOpt {QStringList () << "p" << "port",
                              QStringLiteral ("Inhibit UDP port (default 22372)"),
                              QStringLiteral ("port"),
                              QString::number (kDefaultPort)};
  QCommandLineOption stationOpt {QStringList () << "s" << "station",
                                 QStringLiteral ("Badge station id"),
                                 QStringLiteral ("name"),
                                 QStringLiteral ("TEST-KEY")};
  QCommandLineOption bandOpt {QStringList () << "b" << "band",
                              QStringLiteral ("Band field (informational)"),
                              QStringLiteral ("band"),
                              QStringLiteral ("144")};
  QCommandLineOption ttlOpt {QStringList () << "t" << "ttl-ms",
                             QStringLiteral ("hold_timeout_ms on hold/keepalive packets (default 600)"),
                             QStringLiteral ("ms"),
                             QString::number (kDefaultHoldTimeoutMs)};
  QCommandLineOption fixedHangOpt {
    QStringList () << "fixed-hang-ms",
    QStringLiteral ("Override hang after KEY open (ms). Omit for KEYing-monitor hang."),
    QStringLiteral ("ms")};
  QCommandLineOption globalKeysOpt {
    QStringList () << "global-keys",
    QStringLiteral ("Read grave/` key system-wide (other windows can trigger KEY). Not default.")};
  QCommandLineOption quietKaOpt {
    QStringList () << "verbose-keepalive",
    QStringLiteral ("Log every keepalive (default: HOLD, KEY events, RELEASE only).")};
  parser.addOption (hostOpt);
  parser.addOption (portOpt);
  parser.addOption (stationOpt);
  parser.addOption (bandOpt);
  parser.addOption (ttlOpt);
  parser.addOption (fixedHangOpt);
  parser.addOption (globalKeysOpt);
  parser.addOption (quietKaOpt);
  parser.process (app);

  QString const host = parser.value (hostOpt);
  quint16 const port = static_cast<quint16> (parser.value (portOpt).toUInt ());
  QString const station = parser.value (stationOpt);
  QString const band = parser.value (bandOpt);
  int const hold_timeout_ms = parser.value (ttlOpt).toInt ();
  bool const fixed_hang_set = parser.isSet (fixedHangOpt);
  int const fixed_hang_ms = fixed_hang_set ? parser.value (fixedHangOpt).toInt () : 0;
  bool const global_keys = parser.isSet (globalKeysOpt);
  bool const verbose_ka = parser.isSet (quietKaOpt);
  if (hold_timeout_ms < 100 || hold_timeout_ms > 30000)
    {
      QTextStream err (stderr);
      err << "ttl-ms (hold_timeout_ms) must be 100..30000\n";
      return 2;
    }

  InputMode input_mode = global_keys ? InputMode::GlobalKeys : InputMode::StdinTerminal;

  QString input_note;
  bool use_true_key_level = false; // OS key state (no stdin 120ms glue)
#if defined (Q_OS_LINUX)
  // Prefer EVIOCGKEY for true KEY_GRAVE level (accurate short marks).
  if (open_evdev_keyboard ())
    {
      use_true_key_level = true;
      if (input_mode == InputMode::GlobalKeys)
        {
          input_note = QStringLiteral (
              "KEY = grave/` via /dev/input EVIOCGKEY (true level, system-wide). "
              "Group 'input' may be required.");
        }
      else
        {
          // Still use true level; rare KEY + quit via this TTY.
          (void) setup_stdin_raw (); // q/Esc from this terminal
          input_note = QStringLiteral (
              "KEY = grave/` via /dev/input EVIOCGKEY (true level). "
              "q/Esc only from *this terminal* (not global). May need group 'input'. "
              "--global-keys makes q/Esc system-wide too.");
        }
    }
  else
    {
      if (setup_stdin_raw ())
        {
          input_note = QStringLiteral (
              "WARNING: no /dev/input keyboard — KEY uses stdin sticky (%1 ms). "
              "Add user to group 'input' for true-level KEY (short marks).")
              .arg (kStdinStickyMs);
        }
      else
        {
          input_note = QStringLiteral (
              "ERROR: no /dev/input and no raw stdin. KEY timing unavailable.");
        }
    }
#elif defined (Q_OS_UNIX)
  if (setup_stdin_raw ())
    {
      input_note = QStringLiteral (
          "KEY = grave/` via stdin sticky (%1 ms) — no Linux evdev on this build.")
          .arg (kStdinStickyMs);
    }
#elif defined (Q_OS_WIN)
  use_true_key_level = true; // GetAsyncKeyState is true level
  if (input_mode == InputMode::StdinTerminal)
    {
      input_note = QStringLiteral (
          "Windows: KEY = `~ (VK_OEM_3) true level when *this console* is foreground.");
    }
  else
    {
      input_note = QStringLiteral (
          "Windows: KEY = `~ (VK_OEM_3) true level SYSTEM-WIDE.");
    }
#else
  input_note = QStringLiteral ("This platform has limited KEY-key support.");
#endif
  Q_UNUSED (use_true_key_level);

  QUdpSocket sock;
  qint64 seq = 1;
  KeyingMonitor keying;   // KEYing monitor SM
  int holds_sent = 0;
  int keepalives_sent = 0;
  int releases_sent = 0;

  bool key_down = false;
  bool hold_active = false;   // Hold sender SM
  qint64 hang_until_ms = -1;  // EOT after KEY open (break-in hang)

  auto now_ms = [] () { return QDateTime::currentMSecsSinceEpoch (); };

  QTimer keepalive;
  keepalive.setInterval (kKeepaliveMs);

  // Hold sender: only path that emits UDP (race-safe vs keepalives).
  auto send_hold_packet = [&] (int ttl, bool is_keepalive) {
    if (is_keepalive && !hold_active)
      {
        return; // END_HOLD already cleared hold_active
      }
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

  // END_HOLD: cancel keepalives first, then ttl_ms=0 (docs §3.6).
  auto end_hold = [&] (char const * reason) {
    hang_until_ms = -1;
    if (!hold_active)
      {
        return;
      }
    hold_active = false;
    keepalive.stop ();
    send_hold_packet (0, false);
    QTextStream out (stdout);
    out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
        << "  RELEASE (" << reason << ")\n";
    out.flush ();
  };

  auto start_hold = [&] () {
    if (hold_active)
      {
        return;
      }
    hold_active = true;
    send_hold_packet (hold_timeout_ms, false);
    keepalive.start ();
  };

  // 5 ms poll when true key level is available (evdev / Windows).
  int const poll_ms =
#if defined (Q_OS_LINUX)
      (g_evdev_fd >= 0) ? 5 : 10;
#elif defined (Q_OS_WIN)
      5;
#else
      10;
#endif

  QTextStream out (stdout);
  out << "inhibit-spacebar (KEY agent) → " << host << ':' << port << '\n'
      << "  ` (grave) = KEY level (assert / open)  — not Space\n"
      << "  q or Esc  = release hold and quit\n"
      << "  station=" << station << "  band=" << band << '\n'
      << "  hold_timeout_ms=" << hold_timeout_ms
      << "  keepalive every " << kKeepaliveMs << " ms while hold active\n"
      << "  key poll=" << poll_ms << " ms"
#if defined (Q_OS_LINUX)
      << (g_evdev_fd >= 0 ? " (EVIOCGKEY true level)" : " (stdin sticky fallback)")
#endif
      << '\n'
      << "  hang=" << (fixed_hang_set
                       ? QStringLiteral ("fixed override %1 ms").arg (std::max (0, fixed_hang_ms))
                       : QStringLiteral ("KEYing monitor: break-in 1.5×word gap; continuous hang=0"))
      << '\n'
      << "  " << input_note << '\n'
      << "  See docs/TX_INHIBIT.md §3 (Hold sender + KEYing monitor).\n\n";
  out.flush ();

  QObject::connect (&keepalive, &QTimer::timeout, &app, [&] () {
      if (hold_active)
        {
          send_hold_packet (hold_timeout_ms, true);
        }
    });

  QTimer poll;
  poll.setInterval (poll_ms);
  QObject::connect (&poll, &QTimer::timeout, &app, [&] () {
      qint64 t = now_ms ();

      bool want_quit = false;
      bool space = false;
#if defined (Q_OS_LINUX)
      // KEY (grave): true level via EVIOCGKEY when available.
      // Quit (q/Esc): only this terminal's stdin unless --global-keys, so typing
      // q in another window does not end the agent.
      if (g_evdev_fd >= 0)
        {
          space = space_down_evdev ();
          if (input_mode == InputMode::GlobalKeys)
            {
              want_quit = quit_requested_evdev () || quit_requested_stdin ();
            }
          else
            {
              want_quit = quit_requested_stdin ();
            }
        }
      else
        {
          space = space_down_stdin (t);
          want_quit = quit_requested_stdin ();
        }
#elif defined (Q_OS_UNIX)
      space = space_down_stdin (t);
      want_quit = quit_requested_stdin ();
#elif defined (Q_OS_WIN)
      if (input_mode == InputMode::StdinTerminal)
        {
          space = space_down_console ();
          want_quit = quit_requested_console (); // q/Esc only if console focused
        }
      else
        {
          space = space_down_global ();
          want_quit = quit_requested_global (); // system-wide q/Esc with --global-keys
        }
#else
      Q_UNUSED (t);
#endif

      if (want_quit)
        {
          end_hold ("quit");
          out << "quit\n";
          out.flush ();
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
          restore_stdin_termios ();
#endif
          QCoreApplication::quit ();
          return;
        }

      // --- KEY assert edge → Hold sender + KEYing monitor
      if (space && !key_down)
        {
          key_down = true;
          hang_until_ms = -1;
          keying.on_key_assert (t);
          start_hold ();
          out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
              << "  KEY ASSERT  class=" << keying.class_name () << '\n';
          out.flush ();
        }

      // --- KEY open edge → hang (or EOT if hang=0)
      if (!space && key_down)
        {
          key_down = false;
          int hang_ms = keying.on_key_open (t);
          if (fixed_hang_set)
            {
              hang_ms = std::max (0, fixed_hang_ms);
            }
          QString const cls = QString::fromUtf8 (keying.class_name ());
          double const dit = keying.dit_ms ();
          if (hang_ms <= 0)
            {
              // Continuous KEY / hang 0: EOT immediately (may be before first keepalive)
              end_hold (keying.class_name ());
              keying.reset_transmission ();
              out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
                  << "  KEY OPEN   hang=0  (" << cls << ")\n";
              out.flush ();
            }
          else
            {
              hang_until_ms = t + hang_ms;
              out << QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
                  << "  KEY OPEN   hang=" << hang_ms << " ms"
                  << "  class=" << cls;
              if (dit > 0.0)
                {
                  out << "  dit≈" << qRound (dit) << " ms"
                      << "  ~" << qRound (1200.0 / dit) << " WPM";
                }
              out << '\n';
              out.flush ();
            }
        }

      // --- hang complete → EOT → release hold
      if (!key_down && hang_until_ms >= 0 && t >= hang_until_ms)
        {
          end_hold ("hang done / EOT");
          keying.reset_transmission ();
        }
    });
  poll.start ();

  int rc = app.exec ();
#if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
  restore_stdin_termios ();
#endif
  return rc;
}
