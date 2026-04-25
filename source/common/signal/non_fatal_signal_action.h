#pragma once

#include <unistd.h>

#include <atomic>
#include <csignal>

#include "source/common/common/non_copyable.h"

namespace Envoy {
/**
 * Installs a signal handler for SIGUSR2 that dispatches to registered
 * NonFatalSignalCallback functions.
 */
class NonFatalSignalAction : NonCopyable {
public:
  NonFatalSignalAction() { installSigHandler(); }
  ~NonFatalSignalAction() { removeSigHandler(); }
  static bool isInstalled();
  static void sigHandler(int sig, siginfo_t* info, void* context);

private:
  void installSigHandler();
  void removeSigHandler();

  static std::atomic<bool> installed_;
  struct sigaction previous_handler_;
};
} // namespace Envoy
