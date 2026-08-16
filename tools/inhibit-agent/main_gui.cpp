// inhibit-agent-gui — operator window. No required serial/addr.
// Auto-picks a Keyline (or the only USB-serial) and 127.0.0.1:22372.
// Optional overrides: --port / --addr (same as CLI).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "agent.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString state_stylesheet (AgentState s)
{
  switch (s)
    {
    case AgentState::Open:
      return QStringLiteral (
          "QLabel { background: #d8f0d8; color: #145214; border: 2px solid #2a7a2a; }");
    case AgentState::Inhibiting:
      return QStringLiteral (
          "QLabel { background: #f5c4c4; color: #7a1010; border: 2px solid #a01818; }");
    case AgentState::Hang:
      return QStringLiteral (
          "QLabel { background: #ffe4b8; color: #7a4a00; border: 2px solid #c07800; }");
    case AgentState::SenseFault:
      return QStringLiteral (
          "QLabel { background: #ffe566; color: #5a2000; border: 2px solid #c04000; }");
    }
  return {};
}

} // namespace

int main (int argc, char * argv[])
{
  QApplication app (argc, argv);
  QApplication::setApplicationName (QStringLiteral ("inhibit-agent"));
  QApplication::setApplicationVersion (QStringLiteral ("1.0"));

  QCommandLineParser parser;
  parser.setApplicationDescription (
      QStringLiteral (
          "SSB/CW KEY (USB-serial CTS) -> TX Inhibit hold.\n"
          "GUI needs no arguments: auto-selects Keyline + 127.0.0.1:22372."));
  parser.addHelpOption ();
  parser.addVersionOption ();
  QCommandLineOption portOpt {
    QStringList () << "p" << "port",
    QStringLiteral ("Override USB-serial device"),
    QStringLiteral ("device")};
  QCommandLineOption addrOpt {
    QStringList () << "a" << "addr",
    QStringLiteral ("Override gate address (host:port)"),
    QStringLiteral ("host:port")};
  QCommandLineOption invertOpt {
    QStringList () << "invert",
    QStringLiteral ("Invert CTS polarity")};
  parser.addOption (portOpt);
  parser.addOption (addrOpt);
  parser.addOption (invertOpt);
  parser.process (app);

  InhibitAgent::Config cfg;
  cfg.invert = parser.isSet (invertOpt);
  QString pick_note;
  if (parser.isSet (portOpt))
    {
      cfg.serial_port = parser.value (portOpt);
      pick_note = QStringLiteral ("port from --port");
    }
  else
    {
      cfg.serial_port = InhibitAgent::auto_detect_serial_port (&pick_note);
    }
  if (parser.isSet (addrOpt))
    {
      QString err;
      if (!InhibitAgent::parse_dest_addr (parser.value (addrOpt),
                                          &cfg.dest_host, &cfg.dest_port, &err))
        {
          pick_note = err;
        }
    }

  QWidget win;
  win.setWindowTitle (QStringLiteral ("inhibit-agent"));
  win.resize (520, 360);

  auto * state = new QLabel (QStringLiteral ("STARTING"));
  state->setAlignment (Qt::AlignCenter);
  QFont sf = state->font ();
  sf.setPointSize (28);
  sf.setBold (true);
  state->setFont (sf);
  state->setMinimumHeight (88);
  state->setStyleSheet (state_stylesheet (AgentState::Open));

  auto * info = new QLabel;
  QFont bf = info->font ();
  bf.setPointSize (12);
  info->setFont (bf);
  info->setWordWrap (true);

  auto * log = new QPlainTextEdit;
  log->setReadOnly (true);
  log->setMaximumBlockCount (200);
  QFont lf = log->font ();
  lf.setPointSize (11);
  lf.setFamily (QStringLiteral ("monospace"));
  log->setFont (lf);

  auto * quit = new QPushButton (QStringLiteral ("Quit"));
  quit->setMinimumHeight (32);

  auto * layout = new QVBoxLayout (&win);
  layout->addWidget (state);
  layout->addWidget (info);
  layout->addWidget (log, 1);
  auto * row = new QHBoxLayout;
  row->addStretch (1);
  row->addWidget (quit);
  layout->addLayout (row);

  auto append_log = [log] (QString const& line) {
    log->appendPlainText (
        QDateTime::currentDateTime ().toString (QStringLiteral ("hh:mm:ss.zzz"))
        + QLatin1Char (' ') + line);
  };

  InhibitAgent * agent = new InhibitAgent (cfg, &win);

  auto refresh_info = [info, agent, &pick_note] () {
    info->setText (
        QStringLiteral ("KEY CTS on %1\nGate %2\n%3")
            .arg (agent->serial_port ().isEmpty ()
                  ? QStringLiteral ("(none)")
                  : agent->serial_port ())
            .arg (agent->dest_text ())
            .arg (pick_note));
  };

  QObject::connect (agent, &InhibitAgent::stateChanged, &win,
                    [state] (AgentState s, QString detail) {
                      QString t = InhibitAgent::state_name (s);
                      if (!detail.isEmpty ())
                        {
                          t += QLatin1Char ('\n') + detail;
                        }
                      state->setText (t);
                      state->setStyleSheet (state_stylesheet (s));
                    });
  QObject::connect (agent, &InhibitAgent::logLine, &win, append_log);
  QObject::connect (quit, &QPushButton::clicked, &app, &QCoreApplication::quit);
  QObject::connect (&app, &QCoreApplication::aboutToQuit, agent,
                    [agent] () { agent->stop (); });

  refresh_info ();
  if (cfg.serial_port.isEmpty ())
    {
      state->setText (QStringLiteral ("SENSE FAULT"));
      state->setStyleSheet (state_stylesheet (AgentState::SenseFault));
      append_log (pick_note);
    }
  else if (!agent->start ())
    {
      state->setText (QStringLiteral ("SENSE FAULT"));
      state->setStyleSheet (state_stylesheet (AgentState::SenseFault));
      append_log (agent->fault_reason ());
    }
  else
    {
      append_log (pick_note.isEmpty ()
                  ? QStringLiteral ("started")
                  : pick_note);
    }

  win.show ();
  return app.exec ();
}
