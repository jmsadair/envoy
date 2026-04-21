#include "source/extensions/watchdog/backtrace_action/backtrace_action.h"

#include <sys/syscall.h>

#include "envoy/thread/thread.h"

#include "source/common/protobuf/utility.h"
#include "source/common/signal/non_fatal_signal_action.h"
#include "source/common/signal/non_fatal_signal_handler.h"
#include "source/common/thread/signal_thread.h"

#include "absl/debugging/stacktrace.h"

namespace Envoy {
namespace Extensions {
namespace Watchdog {
namespace BacktraceAction {

BacktraceAction::BacktraceAction(
    envoy::extensions::watchdog::backtrace_action::v3::BacktraceActionConfig& config,
    Server::Configuration::GuardDogActionFactoryContext& context)
    : cooldown_duration_(
          std::chrono::seconds(PROTOBUF_GET_SECONDS_OR_DEFAULT(config, cooldown_duration, 10))),
      handler_registered_(NonFatalSignalHandler::registerNonFatalSignalHandler(*this)) {

  for (int i = 0; i < MaxSlots; ++i) {
    slots_[i].timer = context.dispatcher_.createTimer([this, i]() {
      auto& slot = slots_[i];
      if (slot.ready.load(std::memory_order_acquire)) {
        BackwardsTrace tracer;
        tracer.loadRaw(slot.trace.frames, slot.trace.depth);
        tracer.logTrace();
      }
      slot.tid.store(0, std::memory_order_release);
    });
  }
}

BacktraceAction::~BacktraceAction() {
  if (handler_registered_) {
    NonFatalSignalHandler::removeNonFatalSignalHandler(*this);
  }
}

void BacktraceAction::onNonFatalSignal(int /*sig*/, siginfo_t* /*info*/, void* context) const {
  const pid_t mytid = static_cast<pid_t>(syscall(SYS_gettid));
  for (auto& slot : slots_) {
    if (slot.tid.load(std::memory_order_acquire) == mytid) {
      auto& t = slot.trace;
      if (context != nullptr) {
        t.depth = absl::GetStackTraceWithContext(t.frames, MaxStackDepth, 1, context, nullptr);
      } else {
        t.depth = absl::GetStackTrace(t.frames, MaxStackDepth, 1);
      }
      slot.ready.store(true, std::memory_order_release);
      return;
    }
  }
}

void BacktraceAction::run(
    envoy::config::bootstrap::v3::Watchdog::WatchdogAction::WatchdogEvent /*event*/,
    const std::vector<std::pair<Thread::ThreadId, MonotonicTime>>& thread_last_checkin_pairs,
    MonotonicTime now) {
  if (!handler_registered_) {
    ENVOY_LOG_MISC(warn, "Backtrace Action: signal handler not registered.");
    return;
  }
  if (!NonFatalSignalAction::isInstalled()) {
    ENVOY_LOG_MISC(warn, "Backtrace Action: signal handler not installed.");
    return;
  }
  if (thread_last_checkin_pairs.empty()) {
    ENVOY_LOG_MISC(warn, "Backtrace Action: No tids were provided.");
    return;
  }

  for (const auto& [tid, ltt] : thread_last_checkin_pairs) {
    // Apply cooldown per thread.
    if (auto it = tid_to_last_backtrace_.find(tid); it != tid_to_last_backtrace_.end()) {
      if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second) < cooldown_duration_) {
        continue;
      }
    }

    const pid_t raw_tid = static_cast<pid_t>(tid.getId());

    // Skip if already in-flight for this TID.
    bool pending = false;
    for (const auto& slot : slots_) {
      if (slot.tid.load(std::memory_order_acquire) == raw_tid) {
        pending = true;
        break;
      }
    }
    if (pending) {
      continue;
    }

    // Claim a free slot.
    for (auto& slot : slots_) {
      pid_t expected = 0;
      if (slot.tid.compare_exchange_strong(expected, raw_tid, std::memory_order_release,
                                           std::memory_order_relaxed)) {
        slot.ready.store(false, std::memory_order_relaxed);
        Thread::signalThread(tid, SIGUSR2);
        slot.timer->enableTimer(std::chrono::milliseconds(100));
        tid_to_last_backtrace_[tid] = now;
        break;
      }
    }
  }
}

} // namespace BacktraceAction
} // namespace Watchdog
} // namespace Extensions
} // namespace Envoy
