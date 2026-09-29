#include "UdpDispatch.hpp"

#include <ctime>

#include <QNetworkInterface>
#include <QUdpSocket>

#include "TxInhibit/TxInhibitGate.hpp"
#include "TxInhibit/TxInhibitLogic.hpp"

UdpDispatchWorker::UdpDispatchWorker (QObject * parent)
  : QObject {parent}
{
}

void UdpDispatchWorker::init ()
{
  if (sock_)
    {
      return;
    }
  sock_ = new QUdpSocket {this};
  connect (sock_, &QUdpSocket::readyRead, this, &UdpDispatchWorker::read_pending);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect (sock_, &QUdpSocket::errorOccurred, this, &UdpDispatchWorker::on_error);
#else
  connect (sock_, static_cast<void (QUdpSocket::*) (QAbstractSocket::SocketError)> (&QUdpSocket::error),
           this, &UdpDispatchWorker::on_error);
#endif
}

void UdpDispatchWorker::shutdown ()
{
  if (sock_)
    {
      sock_->disconnect (this);
      sock_->close ();
      delete sock_;
      sock_ = nullptr;
    }
}

void UdpDispatchWorker::do_bind (QString address)
{
  if (!sock_)
    {
      return;
    }
  QHostAddress const addr {address};
  if (sock_->state () != QAbstractSocket::UnconnectedState)
    {
      sock_->close ();
    }
  sock_->bind (addr);
  sock_->setSocketOption (QAbstractSocket::MulticastTtlOption, ttl_);
}

void UdpDispatchWorker::do_close ()
{
  if (sock_ && sock_->state () != QAbstractSocket::UnconnectedState)
    {
      sock_->close ();
    }
}

void UdpDispatchWorker::do_set_ttl (int ttl)
{
  ttl_ = ttl;
  if (sock_)
    {
      sock_->setSocketOption (QAbstractSocket::MulticastTtlOption, ttl_);
    }
}

void UdpDispatchWorker::do_write (QByteArray message, QString address, quint16 port,
                                  bool multicast, QStringList interfaces)
{
  if (!sock_)
    {
      return;
    }
  QHostAddress const dest {address};
  if (!multicast)
    {
      sock_->writeDatagram (message, dest, port);
      return;
    }
  for (auto const& name : interfaces)
    {
      sock_->setMulticastInterface (QNetworkInterface::interfaceFromName (name));
      sock_->writeDatagram (message, dest, port);
    }
}

QString UdpDispatchWorker::read_local_address () const
{
  if (!sock_ || sock_->state () == QAbstractSocket::UnconnectedState)
    {
      return {};
    }
  return sock_->localAddress ().toString ();
}

bool UdpDispatchWorker::is_unconnected () const
{
  return !sock_ || sock_->state () == QAbstractSocket::UnconnectedState;
}

void UdpDispatchWorker::read_pending ()
{
  if (!sock_)
    {
      return;
    }
  while (sock_->hasPendingDatagrams ())
    {
      QByteArray data;
      data.resize (static_cast<int> (sock_->pendingDatagramSize ()));
      if (sock_->readDatagram (data.data (), data.size ()) < 0)
        {
          continue;
        }
      if (TxInhibit::is_tx_inhibit_datagram (data))
        {
          // Stamp at the socket read. First lease sets hold_pending and is
          // applied on the transceiver thread. A refresh does not.
          timespec ts {};
          clock_gettime (CLOCK_MONOTONIC, &ts);
          qint64 const t_rx = static_cast<qint64> (ts.tv_sec) * 1000000000LL
            + static_cast<qint64> (ts.tv_nsec);
          TxInhibitGate::submit_shared (data, t_rx);
        }
      else
        {
          Q_EMIT gui_datagram (data);
        }
    }
}

void UdpDispatchWorker::on_error ()
{
  if (!sock_)
    {
      return;
    }
#if defined (Q_OS_WIN)
  auto const e = sock_->error ();
  // Qt 5.5 reports these spuriously for UDP.
  if (e == QAbstractSocket::NetworkError
      || e == QAbstractSocket::ConnectionRefusedError)
    {
      return;
    }
#endif
  Q_EMIT io_error (sock_->errorString ());
}

UdpDispatch::UdpDispatch ()
{
  worker_ = new UdpDispatchWorker;
  worker_->moveToThread (&thread_);
  connect (worker_, &UdpDispatchWorker::gui_datagram,
           this, &UdpDispatch::gui_datagram, Qt::QueuedConnection);
  connect (worker_, &UdpDispatchWorker::io_error,
           this, &UdpDispatch::io_error, Qt::QueuedConnection);
  thread_.setObjectName (QStringLiteral ("udp-dispatch"));
  thread_.start ();
  QMetaObject::invokeMethod (worker_, "init", Qt::BlockingQueuedConnection);
}

UdpDispatch::~UdpDispatch ()
{
  if (!worker_)
    {
      return;
    }
  // Close the socket on the dispatch thread, then stop the thread and delete
  // the worker. The socket (and its notifier) are already gone, so the worker
  // has nothing left that must be destroyed on that thread.
  QMetaObject::invokeMethod (worker_, "shutdown", Qt::BlockingQueuedConnection);
  thread_.quit ();
  thread_.wait ();
  delete worker_;
  worker_ = nullptr;
}

void UdpDispatch::bind (QHostAddress const& address)
{
  QMetaObject::invokeMethod (worker_, "do_bind", Qt::BlockingQueuedConnection,
                             Q_ARG (QString, address.toString ()));
}

void UdpDispatch::close_socket ()
{
  QMetaObject::invokeMethod (worker_, "do_close", Qt::BlockingQueuedConnection);
}

QString UdpDispatch::local_address () const
{
  QString addr;
  QMetaObject::invokeMethod (worker_, "read_local_address", Qt::BlockingQueuedConnection,
                             Q_RETURN_ARG (QString, addr));
  return addr;
}

bool UdpDispatch::is_unconnected () const
{
  bool unconnected = true;
  QMetaObject::invokeMethod (worker_, "is_unconnected", Qt::BlockingQueuedConnection,
                             Q_RETURN_ARG (bool, unconnected));
  return unconnected;
}

void UdpDispatch::set_ttl (int ttl)
{
  QMetaObject::invokeMethod (worker_, "do_set_ttl", Qt::BlockingQueuedConnection,
                             Q_ARG (int, ttl));
}

void UdpDispatch::send (QByteArray const& message, QHostAddress const& address, quint16 port,
                        bool multicast, QStringList const& interfaces)
{
  QMetaObject::invokeMethod (worker_, "do_write", Qt::BlockingQueuedConnection,
                             Q_ARG (QByteArray, message),
                             Q_ARG (QString, address.toString ()),
                             Q_ARG (quint16, port),
                             Q_ARG (bool, multicast),
                             Q_ARG (QStringList, interfaces));
}
