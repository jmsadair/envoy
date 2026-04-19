#include "source/common/signal/non_fatal_signal_handler.h"

#include <atomic>

#include "source/common/common/logger.h"

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace NonFatalSignalHandler {
namespace {

// Maximum number of signal handlers that may be registered.
constexpr size_t MaxHandlers = 16;

ABSL_CONST_INIT static absl::Mutex non_fatal_signal_mutex(absl::kConstInit);
static std::atomic<const NonFatalSignalHandlerInterface*> non_fatal_signal_handlers[MaxHandlers]{};

} // namespace

bool registerNonFatalSignalHandler(const NonFatalSignalHandlerInterface& handler) {
  absl::MutexLock l(&non_fatal_signal_mutex);

  // Only registers a fixed number of handlers to avoid the need to either acquire
  // the lock or perform memory management in callNonFatalSignalHandlers().
  for (auto& slot : non_fatal_signal_handlers) {
    if (slot == nullptr) {
      slot = &handler;
      return true;
    }
  }

  ENVOY_LOG_MISC(error, "Failed to register non-fatal signal handler: max handlers ({}) reached",
                 MaxHandlers);
  return false;
}

void removeNonFatalSignalHandler(const NonFatalSignalHandlerInterface& handler) {
  absl::MutexLock l(&non_fatal_signal_mutex);
  for (auto& slot : non_fatal_signal_handlers) {
    if (slot.load(std::memory_order_relaxed) == &handler) {
      slot.store(nullptr, std::memory_order_release);
      return;
    }
  }
}

void callNonFatalSignalHandlers(int sig, siginfo_t* info, void* context) {
  for (auto& slot : non_fatal_signal_handlers) {
    const NonFatalSignalHandlerInterface* handler = slot.load(std::memory_order_acquire);
    if (handler != nullptr) {
      handler->onNonFatalSignal(sig, info, context);
    }
  }
}

} // namespace NonFatalSignalHandler
} // namespace Envoy
