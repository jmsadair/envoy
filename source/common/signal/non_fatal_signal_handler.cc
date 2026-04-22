#include "source/common/signal/non_fatal_signal_handler.h"

#include <atomic>

#include "source/common/common/logger.h"

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace NonFatalSignalHandler {

namespace {

ABSL_CONST_INIT static absl::Mutex handler_mutex(absl::kConstInit);
static std::atomic<NonFatalSignalCallback> handlers[MaxHandlers]{};

} // namespace

bool registerNonFatalSignalHandler(NonFatalSignalCallback cb) {
  absl::MutexLock l(&handler_mutex);
  for (auto& slot : handlers) {
    if (slot.load(std::memory_order_relaxed) == nullptr) {
      slot.store(cb, std::memory_order_release);
      return true;
    }
  }
  ENVOY_LOG_MISC(error, "Failed to register non-fatal signal handler: max handlers ({}) reached",
                 MaxHandlers);
  return false;
}

void removeNonFatalSignalHandler(NonFatalSignalCallback cb) {
  absl::MutexLock l(&handler_mutex);
  for (auto& slot : handlers) {
    if (slot.load(std::memory_order_relaxed) == cb) {
      slot.store(nullptr, std::memory_order_release);
      return;
    }
  }
}

void callNonFatalSignalHandlers(int sig, siginfo_t* info, void* context) {
  for (auto& slot : handlers) {
    NonFatalSignalCallback cb = slot.load(std::memory_order_acquire);
    if (cb != nullptr) {
      cb(sig, info, context);
    }
  }
}

} // namespace NonFatalSignalHandler
} // namespace Envoy
