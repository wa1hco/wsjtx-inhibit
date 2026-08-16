#ifndef TX_INHIBIT_SIM_PORT_HPP__
#define TX_INHIBIT_SIM_PORT_HPP__

#include <QString>

// Dummy PTT port name for Settings (Radio None + RTS/DTR). Starts the
// TX Inhibit gate without opening a UART. Not a KEY source.
inline char const * inhibit_sim_port_name ()
{
  return "inhibit-sim";
}

inline bool is_inhibit_sim_port (QString const& port)
{
  return port.compare (QLatin1String (inhibit_sim_port_name ()),
                       Qt::CaseInsensitive) == 0;
}

#endif
