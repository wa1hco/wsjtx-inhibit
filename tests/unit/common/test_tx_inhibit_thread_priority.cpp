#include <QtTest>

#include "TxInhibit/TxInhibitThreadPriority.hpp"

#if defined(__linux__)
#include <sched.h>
#endif

class TestTxInhibitThreadPriority : public QObject
{
  Q_OBJECT

private slots:
  void refusalLeavesCallerAlive ()
  {
    auto const result = raise_inhibit_thread_priority ();
#if defined(Q_OS_WIN)
    QVERIFY (result.raised);
    QCOMPARE (result.requested, THREAD_PRIORITY_HIGHEST);
    QCOMPARE (result.error, 0);
#else
    // A refused rtprio limit must leave this thread runnable.
    if (result.raised)
      {
        QCOMPARE (result.requested, 20);
        QCOMPARE (result.error, 0);
      }
    else
      {
        QVERIFY (result.error != 0);
#if defined(__linux__)
        QCOMPARE (sched_getscheduler (0), SCHED_OTHER);
#endif
      }
#endif
  }
};

QTEST_GUILESS_MAIN (TestTxInhibitThreadPriority)
#include "test_tx_inhibit_thread_priority.moc"
