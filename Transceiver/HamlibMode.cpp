#include "HamlibMode.hpp"

namespace HamlibMode
{
  auto from_hamlib (rmode_t mode) -> Transceiver::MODE
  {
    switch (mode)
      {
      case RIG_MODE_AM:
      case RIG_MODE_SAM:
      case RIG_MODE_AMS:
      case RIG_MODE_DSB:
        return Transceiver::AM;

      case RIG_MODE_CW:
        return Transceiver::CW;

      case RIG_MODE_CWR:
        return Transceiver::CW_R;

      case RIG_MODE_USB:
      case RIG_MODE_ECSSUSB:
      case RIG_MODE_SAH:
      case RIG_MODE_FAX:
        return Transceiver::USB;

      case RIG_MODE_LSB:
      case RIG_MODE_ECSSLSB:
      case RIG_MODE_SAL:
        return Transceiver::LSB;

      case RIG_MODE_RTTY:
        return Transceiver::FSK;

      case RIG_MODE_RTTYR:
        return Transceiver::FSK_R;

      case RIG_MODE_PKTLSB:
#ifdef RIG_MODE_LSBD1
      case RIG_MODE_LSBD1:
#endif
#ifdef RIG_MODE_LSBD2
      case RIG_MODE_LSBD2:
#endif
#ifdef RIG_MODE_LSBD3
      case RIG_MODE_LSBD3:
#endif
        return Transceiver::DIG_L;

      case RIG_MODE_PKTUSB:
#ifdef RIG_MODE_USBD1
      case RIG_MODE_USBD1:
#endif
#ifdef RIG_MODE_USBD2
      case RIG_MODE_USBD2:
#endif
#ifdef RIG_MODE_USBD3
      case RIG_MODE_USBD3:
#endif
        return Transceiver::DIG_U;

      case RIG_MODE_FM:
      case RIG_MODE_WFM:
        return Transceiver::FM;

      case RIG_MODE_PKTFM:
        return Transceiver::DIG_FM;

      default:
        return Transceiver::UNK;
      }
  }

  rmode_t to_hamlib (Transceiver::MODE mode)
  {
    switch (mode)
      {
      case Transceiver::AM: return RIG_MODE_AM;
      case Transceiver::CW: return RIG_MODE_CW;
      case Transceiver::CW_R: return RIG_MODE_CWR;
      case Transceiver::USB: return RIG_MODE_USB;
      case Transceiver::LSB: return RIG_MODE_LSB;
      case Transceiver::FSK: return RIG_MODE_RTTY;
      case Transceiver::FSK_R: return RIG_MODE_RTTYR;
      case Transceiver::DIG_L: return RIG_MODE_PKTLSB;
      case Transceiver::DIG_U: return RIG_MODE_PKTUSB;
      case Transceiver::FM: return RIG_MODE_FM;
      case Transceiver::DIG_FM: return RIG_MODE_PKTFM;
      default: break;
      }
    return RIG_MODE_USB;
  }

  bool satisfies_request (rmode_t requested, rmode_t current)
  {
    if (requested == current)
      {
        return true;
      }

    if (RIG_MODE_PKTUSB == requested)
      {
#if defined(RIG_MODE_USBD1) || defined(RIG_MODE_USBD2) || defined(RIG_MODE_USBD3)
        return
#  ifdef RIG_MODE_USBD1
          RIG_MODE_USBD1 == current
#  endif
#  if defined(RIG_MODE_USBD2)
#    ifdef RIG_MODE_USBD1
          ||
#    endif
          RIG_MODE_USBD2 == current
#  endif
#  if defined(RIG_MODE_USBD3)
#    if defined(RIG_MODE_USBD1) || defined(RIG_MODE_USBD2)
          ||
#    endif
          RIG_MODE_USBD3 == current
#  endif
          ;
#else
        return false;
#endif
      }

    if (RIG_MODE_PKTLSB == requested)
      {
#if defined(RIG_MODE_LSBD1) || defined(RIG_MODE_LSBD2) || defined(RIG_MODE_LSBD3)
        return
#  ifdef RIG_MODE_LSBD1
          RIG_MODE_LSBD1 == current
#  endif
#  if defined(RIG_MODE_LSBD2)
#    ifdef RIG_MODE_LSBD1
          ||
#    endif
          RIG_MODE_LSBD2 == current
#  endif
#  if defined(RIG_MODE_LSBD3)
#    if defined(RIG_MODE_LSBD1) || defined(RIG_MODE_LSBD2)
          ||
#    endif
          RIG_MODE_LSBD3 == current
#  endif
          ;
#else
        return false;
#endif
      }

    return false;
  }

  bool change_required (Transceiver::MODE requested, rmode_t current)
  {
    return Transceiver::UNK != requested
      && !satisfies_request (to_hamlib (requested), current);
  }
}
