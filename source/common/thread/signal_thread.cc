#include "source/common/thread/signal_thread.h"

#include <sys/types.h>

#include <csignal>

#include "source/common/common/logger.h"

namespace Envoy {
namespace Thread {
namespace {
#ifdef __linux__
pid_t toPlatformTid(int64_t tid) { return static_cast<pid_t>(tid); }
#elif defined(__APPLE__)
uint64_t toPlatformTid(int64_t tid) { return static_cast<uint64_t>(tid); }
#endif
} // namespace

bool terminateThread(const ThreadId& tid) { return signalThread(tid, SIGABRT); }

bool signalThread(const ThreadId& tid, int signal) {
#ifndef WIN32
  // Assume POSIX-compatible system and signal to the thread.
  return kill(toPlatformTid(tid.getId()), signal) == 0;
#else
  // Windows, currently unsupported termination of thread.
  ENVOY_LOG_MISC(error, "Windows is currently unsupported for signalThread.");
  return false;
#endif
}

} // namespace Thread
} // namespace Envoy
