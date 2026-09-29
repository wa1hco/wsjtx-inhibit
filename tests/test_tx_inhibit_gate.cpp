// Integration tests for TxInhibitGate: the intent/hold interaction and the
// gate lifecycle, over a real UDP socket and a real event loop.
//
// test_tx_inhibit_logic covers GateLogic, which is pure and takes injected
// time. Nothing covered the part a rebase is most likely to break: the wiring
// between do_ptt's intent, the UDP hold, and the physicalPtt emission that
// drives the radio. That code lives in upstream-owned files
// (HamlibTransceiver, Configuration), so it is the most exposed to upstream
// churn -- and a break there is silent until someone with a radio notices.
//
// See docs/REVIEW-rc2.md H6.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QUdpSocket>

#include "Network/MessageClient.hpp"
#include "TxInhibit/TxInhibitGate.hpp"
#include "TxInhibit/TxInhibitLogic.hpp"

namespace
{
  QByteArray hold_packet (int ttl_ms, char const * station = "TEST"
                          , char const * controller = "test-agent")
  {
    return TxInhibit::build_datagram (QString::fromUtf8 (controller)
                                      , static_cast<quint32> (ttl_ms)
                                      , QString::fromUtf8 (station));
  }
}

class TestTxInhibitGate final
  : public QObject
{
  Q_OBJECT

private slots:

  // The core equation, end to end: assert PTT <=> want_tx and not hold.
  // Crucially, want_tx never changes here -- only the hold does. That is the
  // whole point of the feature, and the thing a careless refactor breaks.
  void pinFollowsHoldWhileIntentStaysOn ()
  {
    TxInhibitGate gate;
    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);

    // want_tx on, no hold -> assert
    gate.set_intent (true);
    QCOMPARE (pin.count (), 1);
    QCOMPARE (pin.at (0).at (0).toBool (), true);

    // hold arrives; want_tx is untouched -> release
    TxInhibitGate::submit_shared (hold_packet (5000));
    QTRY_COMPARE (pin.count (), 2);
    QCOMPARE (pin.at (1).at (0).toBool (), false);

    // explicit release; want_tx still on -> assert again
    TxInhibitGate::submit_shared (hold_packet (0));
    QTRY_COMPARE (pin.count (), 3);
    QCOMPARE (pin.at (2).at (0).toBool (), true);

    gate.shutdown (false);
  }

  // Deadman: an agent that dies without releasing must not hold PTT off
  // forever. The station's own hold timeout has to clear it.
  void holdTimeoutRecoversWithoutRelease ()
  {
    TxInhibitGate gate;
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);

    gate.set_intent (true);
    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};

    TxInhibitGate::submit_shared (hold_packet (TxInhibit::hold_timeout_ms_min));
    QTRY_COMPARE (pin.count (), 1);
    QCOMPARE (pin.at (0).at (0).toBool (), false);   // held

    // No release sent. The hold must expire on its own.
    QTRY_COMPARE (pin.count (), 2);
    QCOMPARE (pin.at (1).at (0).toBool (), true);    // recovered

    gate.shutdown (false);
  }

  // A hold with no transmit intent must not assert PTT when it clears.
  void releaseWithoutIntentDoesNotKey ()
  {
    TxInhibitGate gate;
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);

    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};
    TxInhibitGate::submit_shared (hold_packet (200));
    QTest::qWait (400);                      // hold applied and expired
    TxInhibitGate::submit_shared (hold_packet (0));
    QTest::qWait (100);

    QCOMPARE (pin.count (), 0);              // never keyed: want_tx was false
    gate.shutdown (false);
  }

  // Badge text and counters reach the GUI, and only on a real change.
  void reportsStateChangesOnce ()
  {
    TxInhibitGate gate;
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);

    QSignalSpy changed {&gate, &TxInhibitGate::inhibitChanged};
    TxInhibitGate::submit_shared (hold_packet (5000, "W1AW"));
    QTRY_VERIFY (changed.count () >= 1);
    QCOMPARE (changed.at (0).at (0).toBool (), true);
    QVERIFY (changed.at (0).at (1).toString ().contains (QStringLiteral ("W1AW")));

    // A keepalive refreshes the timeout but is not a state change.
    int const before = changed.count ();
    TxInhibitGate::submit_shared (hold_packet (5000, "W1AW"));
    QTest::qWait (100);
    QCOMPARE (changed.count (), before);

    gate.shutdown (false);
  }

  // shutdown(false) must not request a pin change: Hamlib may already be
  // closed, and rig_set_ptt then fails. This is the teardown ordering that
  // HamlibTransceiver::stop_tx_inhibit_gate depends on.
  void shutdownWithoutPinEmitIsSilent ()
  {
    TxInhibitGate gate;
    gate.start_listening ();
    gate.set_intent (true);

    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};
    gate.shutdown (false);
    QCOMPARE (pin.count (), 0);

    // And nothing may be emitted after shutdown, whatever arrives.
    gate.set_intent (true);
    QTest::qWait (60);
    QCOMPARE (pin.count (), 0);
  }

  // Hold expiry must be measured on a monotonic base, so that a system-clock
  // step cannot extend or curtail a hold. WSJT-X hosts step their clocks
  // routinely (Meinberg, Dimension4, BktTimeSync). We cannot move the system
  // clock from a test, so assert the property that makes the gate immune:
  // its time base advances with elapsed time and is independent of the wall
  // clock's absolute value.
  void holdTimingUsesMonotonicBase ()
  {
    TxInhibitGate gate;
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);
    gate.set_intent (true);

    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};

    QElapsedTimer measured;
    measured.start ();
    TxInhibitGate::submit_shared (hold_packet (300));
    QTRY_COMPARE (pin.count (), 1);          // held
    QTRY_COMPARE (pin.count (), 2);          // expired
    auto const elapsed = measured.elapsed ();

    // Generous bounds: this asserts "the timeout tracks elapsed time", not a
    // precise duration. A wall-clock base would still pass here on a quiet
    // machine -- the real protection is that now_ms() uses QElapsedTimer, and
    // this test fails loudly if that timing is ever wired to something that
    // does not advance.
    QVERIFY2 (elapsed >= 250 && elapsed < 3000,
              qPrintable (QStringLiteral ("hold lasted %1 ms, expected ~300")
                          .arg (elapsed)));

    gate.shutdown (false);
  }

  // Heartbeat-source type 18: the first lease is pending until this thread
  // applies it. A refresh of that lease is not pending.
  void sharedSocketFirstLeaseIsPendingRefreshIsNot ()
  {
    TxInhibitGate gate;
    gate.set_instance_id (QStringLiteral ("WSJT-X"));
    gate.start_listening ();
    TxInhibitGate::set_commands_enabled (true);

    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};
    gate.set_intent (true);
    QCOMPARE (pin.count (), 1);
    QCOMPARE (pin.at (0).at (0).toBool (), true);

    TxInhibitGate::submit_shared (hold_packet (5000, "W1AW", "KEY"));
    QVERIFY (gate.hold_pending ());
    QTRY_COMPARE (pin.count (), 2);
    QCOMPARE (pin.at (1).at (0).toBool (), false);
    QVERIFY (!gate.hold_pending ());

    TxInhibitGate::submit_shared (hold_packet (5000, "W1AW", "KEY"));
    QVERIFY (!gate.hold_pending ());
    QTest::qWait (50);
    QCOMPARE (pin.count (), 2);

    auto const other = TxInhibit::build_datagram (QStringLiteral ("KEY"), 5000,
                                                   QStringLiteral ("W1AW"),
                                                   QStringLiteral ("OTHER"));
    TxInhibitGate::submit_shared (other);
    QVERIFY (!gate.hold_pending ());

    gate.shutdown (false);
  }

  // The dispatch thread reads the heartbeat socket and delivers an initiating
  // type 18 to the gate without a GUI parse.
  void dispatchThreadDeliversFirstLease ()
  {
    QUdpSocket server;
    QVERIFY (server.bind (QHostAddress {QHostAddress::LocalHost}, quint16 {0}));

    TxInhibitGate gate;
    gate.set_instance_id (QStringLiteral ("WSJT-X"));
    gate.start_listening ();
    QSignalSpy pin {&gate, &TxInhibitGate::physicalPtt};
    gate.set_intent (true);
    QCOMPARE (pin.count (), 1);
    TxInhibitGate::set_commands_enabled (true);

    MessageClient client {QStringLiteral ("WSJT-X"), QStringLiteral ("t"),
                          QStringLiteral ("t"), QStringLiteral ("127.0.0.1"),
                          server.localPort (), {}, 1};
    QTRY_VERIFY (server.hasPendingDatagrams ());
    QByteArray buf;
    buf.resize (server.pendingDatagramSize ());
    QHostAddress from;
    quint16 from_port = 0;
    QCOMPARE (server.readDatagram (buf.data (), buf.size (), &from, &from_port), buf.size ());
    QVERIFY (from_port != 0);

    QUdpSocket agent;
    agent.writeDatagram (hold_packet (5000, "W1AW", "KEY"), QHostAddress::LocalHost, from_port);
    QTRY_COMPARE (pin.count (), 2);
    QCOMPARE (pin.at (1).at (0).toBool (), false);

    gate.shutdown (false);
  }
};

QTEST_GUILESS_MAIN (TestTxInhibitGate)
#include "test_tx_inhibit_gate.moc"
