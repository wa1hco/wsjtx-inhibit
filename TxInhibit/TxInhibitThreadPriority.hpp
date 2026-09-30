#ifndef TX_INHIBIT_THREAD_PRIORITY_HPP__
#define TX_INHIBIT_THREAD_PRIORITY_HPP__

// Raise the calling thread. Call this on the udp-dispatch thread.
//
// Linux uses SCHED_FIFO 20. That sits under PipeWire (95) and USRP (99).
// The thread sleeps in the UDP socket, so the high priority runs only
// when a datagram arrives. A refused rtprio limit leaves normal priority.
// Windows uses THREAD_PRIORITY_HIGHEST inside the current process class.
// The process class stays unchanged.

#include <cerrno>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  ifdef interface
#    undef interface
#  endif
#elif defined(__linux__)
#  include <pthread.h>
#  include <sched.h>
#  include <sys/resource.h>
#endif

struct TxInhibitThreadPriority
{
  bool raised {false};
  // Linux: SCHED_FIFO priority. Windows: THREAD_PRIORITY_* value.
  int requested {0};
  // Linux: pthread error number. Windows: GetLastError. Zero on success.
  int error {0};
};

inline TxInhibitThreadPriority raise_inhibit_thread_priority ()
{
  TxInhibitThreadPriority result;
#if defined(_WIN32)
  result.requested = THREAD_PRIORITY_HIGHEST;
  if (SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_HIGHEST))
    {
      result.raised = true;
    }
  else
    {
      result.error = static_cast<int> (GetLastError ());
    }
#elif defined(__linux__)
  // Stay under audio and radio USB threads. 20 is enough to beat a
  // loaded SCHED_OTHER compile.
  constexpr int k_fifo = 20;
  result.requested = k_fifo;
  rlimit lim {};
  if (getrlimit (RLIMIT_RTPRIO, &lim) == 0
      && lim.rlim_cur < static_cast<rlim_t> (k_fifo)
      && (lim.rlim_max >= static_cast<rlim_t> (k_fifo) || lim.rlim_max == RLIM_INFINITY))
    {
      lim.rlim_cur = static_cast<rlim_t> (k_fifo);
      setrlimit (RLIMIT_RTPRIO, &lim);
    }
  sched_param param {};
  param.sched_priority = k_fifo;
  int const rc = pthread_setschedparam (pthread_self (), SCHED_FIFO, &param);
  if (rc == 0)
    {
      result.raised = true;
    }
  else
    {
      result.error = rc;
    }
#else
  result.error = ENOTSUP;
#endif
  return result;
}

#endif
