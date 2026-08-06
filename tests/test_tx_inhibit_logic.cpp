#include <QtTest>

#include "TxInhibit/TxInhibitLogic.hpp"

class TestTxInhibitLogic : public QObject
{
  Q_OBJECT

private slots:
  void radiateIsIntentAndNotHold ()
  {
    TxInhibit::GateLogic g;
    qint64 t = 1000000;
    QVERIFY (g.radiate (true, t));
    QVERIFY (!g.radiate (false, t));

    // Hold for 500 ms
    QByteArray hold = R"({"tx_inhibit":1,"ttl_ms":500,"station":"TEST","seq":1})";
    QVERIFY (g.on_datagram (hold, t));
    QVERIFY (g.line_inhibited (t));
    QVERIFY (!g.radiate (true, t));
    QVERIFY (!g.radiate (false, t));

    // After deadline: free again
    QVERIFY (!g.line_inhibited (t + 501));
    QVERIFY (g.radiate (true, t + 501));
  }

  void releaseClearsHold ()
  {
    TxInhibit::GateLogic g;
    qint64 t = 2000000;
    QByteArray hold = R"({"tx_inhibit":1,"ttl_ms":1000,"station":"KEY","seq":2})";
    QVERIFY (g.on_datagram (hold, t));
    QVERIFY (!g.radiate (true, t));

    QByteArray rel = R"({"tx_inhibit":1,"ttl_ms":0,"station":"KEY","seq":3})";
    QVERIFY (g.on_datagram (rel, t + 10));
    QVERIFY (g.radiate (true, t + 10));
  }
};

QTEST_MAIN (TestTxInhibitLogic)
#include "test_tx_inhibit_logic.moc"
