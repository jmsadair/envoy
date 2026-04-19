#include "source/common/signal/non_fatal_signal_action.h"

#include <csignal>

#include "source/common/common/assert.h"
#include "source/common/signal/non_fatal_signal_handler.h"

namespace Envoy {

std::atomic<bool> NonFatalSignalAction::installed_ = false;

void NonFatalSignalAction::sigHandler(int sig, siginfo_t* info, void* context) {
  NonFatalSignalHandler::callNonFatalSignalHandlers(sig, info, context);
}

bool NonFatalSignalAction::isInstalled() { return installed_; }

void NonFatalSignalAction::installSigHandler() {
  struct sigaction saction;
  std::memset(&saction, 0, sizeof(saction));
  sigemptyset(&saction.sa_mask);
  saction.sa_flags = SA_SIGINFO;
  saction.sa_sigaction = sigHandler;
  RELEASE_ASSERT(sigaction(SIGUSR2, &saction, &previous_handler_) == 0, "");
  installed_ = true;
}

void NonFatalSignalAction::removeSigHandler() {
  RELEASE_ASSERT(sigaction(SIGUSR2, &previous_handler_, nullptr) == 0, "");
  installed_ = false;
}

} // namespace Envoy
