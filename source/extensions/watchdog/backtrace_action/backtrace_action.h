#pragma once

#include <array>
#include <atomic>
#include <chrono>

#include "envoy/event/timer.h"
#include "envoy/extensions/watchdog/backtrace_action/v3/backtrace_action.pb.h"
#include "envoy/server/guarddog_config.h"
#include "envoy/thread/thread.h"

#include "source/common/signal/non_fatal_signal_handler.h"
#include "source/server/backtrace.h"

namespace Envoy {
namespace Extensions {
namespace Watchdog {
namespace BacktraceAction {

/**
 * A GuardDogAction that will log backtraces of stuck threads.
 */
class BacktraceAction : public Server::Configuration::GuardDogAction,
                        public NonFatalSignalHandlerInterface {
public:
  BacktraceAction(envoy::extensions::watchdog::backtrace_action::v3::BacktraceActionConfig& config,
                  Server::Configuration::GuardDogActionFactoryContext& context);
  ~BacktraceAction() override;

  void run(envoy::config::bootstrap::v3::Watchdog::WatchdogAction::WatchdogEvent event,
           const std::vector<std::pair<Thread::ThreadId, MonotonicTime>>& thread_last_checkin_pairs,
           MonotonicTime now) override;

  void onNonFatalSignal(int sig, siginfo_t* info, void* context) const override;

private:
  static constexpr int MaxSlots = 16;
  static constexpr int MaxStackDepth = BackwardsTrace::MaxStackDepth;

  struct RawTrace {
    void* frames[MaxStackDepth];
    int depth{0};
  };

  struct Slot {
    std::atomic<pid_t> tid{0};
    std::atomic<bool> ready{false};
    RawTrace trace{};
    Event::TimerPtr timer;
  };

  std::chrono::seconds cooldown_duration_;
  absl::flat_hash_map<Thread::ThreadId, MonotonicTime> tid_to_last_backtrace_;
  bool handler_registered_;
  mutable std::array<Slot, MaxSlots> slots_;
};

using BacktraceActionPtr = std::unique_ptr<BacktraceAction>;

} // namespace BacktraceAction
} // namespace Watchdog
} // namespace Extensions
} // namespace Envoy
