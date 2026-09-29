#ifndef UDP_DISPATCH_HPP__
#define UDP_DISPATCH_HPP__

// Thread that owns the WSJT-X UDP socket (the heartbeat source).
//
// Every inbound datagram is read here, off the GUI thread. Type 18
// (TxInhibit) is handed to TxInhibitGate. Every other type is queued
// back to MessageClient for the existing parsers.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

class QUdpSocket;

class UdpDispatchWorker final
  : public QObject
{
  Q_OBJECT

public:
  explicit UdpDispatchWorker (QObject * parent = nullptr);

public slots:
  void init ();
  void shutdown ();
  void do_bind (QString address);
  void do_close ();
  void do_set_ttl (int ttl);
  void do_write (QByteArray message, QString address, quint16 port,
                 bool multicast, QStringList interfaces);
  QString read_local_address () const;
  bool is_unconnected () const;

signals:
  void gui_datagram (QByteArray data);
  void io_error (QString message);

private slots:
  void read_pending ();
  void on_error ();

private:
  QUdpSocket * sock_ {nullptr};
  int ttl_ {1};
};

// Lives on the GUI thread. The worker and its QUdpSocket live on thread_.
class UdpDispatch final
  : public QObject
{
  Q_OBJECT

public:
  UdpDispatch ();
  ~UdpDispatch () override;

  void bind (QHostAddress const& address);
  void close_socket ();
  QString local_address () const;
  bool is_unconnected () const;
  void set_ttl (int ttl);
  void send (QByteArray const& message, QHostAddress const& address, quint16 port,
             bool multicast, QStringList const& interfaces);

signals:
  void gui_datagram (QByteArray const& data);
  void io_error (QString const& message);

private:
  QThread thread_;
  UdpDispatchWorker * worker_ {nullptr};
};

#endif
