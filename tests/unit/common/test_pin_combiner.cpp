#include <QtTest>

#include "TxInhibit/TxInhibitDrop.hpp"

#if defined(Q_OS_UNIX)
#include <cstring>
#include <dirent.h>
#include <string>
#include <pty.h>
#include <unistd.h>
#endif

// The separate PTT port opens on selection and stays open across pin edges.
class TestPinCombiner final : public QObject
{
  Q_OBJECT

private slots:
  void separate_port_stays_open_while_selected_ ();
  void later_publish_closes_the_old_port_ ();
  void separate_pty_does_not_fake_a_stamp_ ();
};

#if defined(Q_OS_UNIX)
namespace
{
  struct Pty
  {
    int master {-1};
    int slave {-1};
    char name[128] {};

    bool open ()
    {
      return ::openpty (&master, &slave, name, nullptr, nullptr) == 0;
    }

    ~Pty ()
    {
      if (master >= 0) ::close (master);
      if (slave >= 0) ::close (slave);
    }
  };

  // Count this process's descriptors whose target is path.
  int count_fd_path (char const * path)
  {
    int count = 0;
    DIR * dir = ::opendir ("/proc/self/fd");
    if (!dir) return -1;
    while (dirent * ent = ::readdir (dir))
      {
        if (ent->d_name[0] == '.') continue;
        std::string const link_path = std::string ("/proc/self/fd/") + ent->d_name;
        char target[256];
        ssize_t const n = ::readlink (link_path.c_str (), target, sizeof target - 1);
        if (n < 0) continue;
        target[n] = '\0';
        if (std::strcmp (target, path) == 0) ++count;
      }
    ::closedir (dir);
    return count;
  }
}
#endif

void TestPinCombiner::separate_port_stays_open_while_selected_ ()
{
#if !defined(Q_OS_UNIX)
  QSKIP ("The separate PTT hold uses a Unix file descriptor.");
#else
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::publish_separate (pty.name, TIOCM_RTS);
  int const fd = TxInhibitDrop::line_fd ().load ();
  QVERIFY (fd >= 0);
  // openpty holds the slave too, so the inhibit open is the second.
  QCOMPARE (count_fd_path (pty.name), 2);

  TxInhibitDrop::set_ptt_intent (true);
  TxInhibitDrop::set_inhibit_here (false);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  TxInhibitDrop::set_inhibit_here (true);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  TxInhibitDrop::set_inhibit_here (false);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  quint64 const epoch = TxInhibitDrop::epoch ().load ();
  TxInhibitDrop::shutdown_here ();
  QCOMPARE (TxInhibitDrop::epoch ().load (), epoch + 1);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), -1);
  QVERIFY (!TxInhibitDrop::separate_port ().load ());
  QCOMPARE (count_fd_path (pty.name), 1);
#endif
}

void TestPinCombiner::later_publish_closes_the_old_port_ ()
{
#if !defined(Q_OS_UNIX)
  QSKIP ("The separate PTT hold uses a Unix file descriptor.");
#else
  TxInhibitDrop::shutdown_here ();
  Pty first;
  Pty second;
  QVERIFY (first.open ());
  QVERIFY (second.open ());
  TxInhibitDrop::publish_separate (first.name, TIOCM_DTR);
  QCOMPARE (count_fd_path (first.name), 2);

  TxInhibitDrop::publish_separate (second.name, TIOCM_DTR);
  QCOMPARE (count_fd_path (first.name), 1);
  QCOMPARE (count_fd_path (second.name), 2);
  QVERIFY (TxInhibitDrop::line_fd ().load () >= 0);

  TxInhibitDrop::shutdown_here ();
  QCOMPARE (count_fd_path (second.name), 1);
  QVERIFY (!TxInhibitDrop::separate_port ().load ());
#endif
}

void TestPinCombiner::separate_pty_does_not_fake_a_stamp_ ()
{
#if !defined(Q_OS_UNIX)
  QSKIP ("The separate PTT hold uses a Unix file descriptor.");
#else
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::set_ptt_intent (true);
  TxInhibitDrop::publish_separate (pty.name, TIOCM_RTS);
  TxInhibitDrop::set_inhibit_here (false);

  qint64 const t_rx = TxInhibitDrop::monotonic_ns ();
  TxInhibitDrop::set_inhibit_here (true, t_rx);
  qint64 got_rx = 1;
  qint64 got_pin = 1;
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {0});
  QCOMPARE (got_pin, qint64 {0});
  TxInhibitDrop::shutdown_here ();
#endif
}

QTEST_GUILESS_MAIN (TestPinCombiner)
#include "test_pin_combiner.moc"
