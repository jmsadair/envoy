#include "source/extensions/watchdog/backtrace_action/backtrace_action.h"

#include <fcntl.h>

#include "envoy/thread/thread.h"

#include "source/common/protobuf/utility.h"
#include "source/common/signal/non_fatal_signal_action.h"
#include "source/common/signal/non_fatal_signal_handler.h"
#include "source/common/thread/signal_thread.h"
#include "source/server/backtrace.h"

#include "absl/debugging/stacktrace.h"

namespace Envoy {
namespace Extensions {
namespace Watchdog {
namespace BacktraceAction {

namespace {
struct RawTrace {
  static constexpr int MaxDepth = BackwardsTrace::MaxStackDepth;
  void* frames[MaxDepth];
  int depth{0};
};
} // namespace

BacktraceAction::BacktraceAction(
    envoy::extensions::watchdog::backtrace_action::v3::BacktraceActionConfig& config,
    Server::Configuration::GuardDogActionFactoryContext& context)
    : cooldown_duration_(
          std::chrono::seconds(PROTOBUF_GET_SECONDS_OR_DEFAULT(config, cooldown_duration, 10))),
      handler_registered_(NonFatalSignalHandler::registerNonFatalSignalHandler(*this)) {
  int fds[2];
  RELEASE_ASSERT(pipe(fds) == 0, "");
  pipe_read_fd_ = fds[0];
  pipe_write_fd_ = fds[1];
  RELEASE_ASSERT(fcntl(pipe_read_fd_, F_SETFL, O_NONBLOCK) == 0, "");
  RELEASE_ASSERT(fcntl(pipe_write_fd_, F_SETFL, O_NONBLOCK) == 0, "");

  pipe_event_ = context.dispatcher_.createFileEvent(
      pipe_read_fd_,
      [this](uint32_t) -> absl::Status {
        RawTrace trace;
        while (read(pipe_read_fd_, &trace, sizeof(trace)) == sizeof(trace)) {
          BackwardsTrace tracer;
          tracer.loadRaw(trace.frames, trace.depth);
          tracer.logTrace();
        }
        return absl::OkStatus();
      },
      Event::PlatformDefaultTriggerType, Event::FileReadyType::Read);
}

BacktraceAction::~BacktraceAction() {
  if (handler_registered_) {
    NonFatalSignalHandler::removeNonFatalSignalHandler(*this);
  }
  close(pipe_write_fd_);
  pipe_event_.reset();
  close(pipe_read_fd_);
}

void BacktraceAction::onNonFatalSignal(int /*sig*/, siginfo_t* /*info*/, void* context) const {
  RawTrace trace;
  if (context != nullptr) {
    trace.depth =
        absl::GetStackTraceWithContext(trace.frames, RawTrace::MaxDepth, 1, context, nullptr);
  } else {
    trace.depth = absl::GetStackTrace(trace.frames, RawTrace::MaxDepth, 1);
  }
  write(pipe_write_fd_, &trace, sizeof(trace));
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

  for (const auto& pair : thread_last_checkin_pairs) {
    if (auto it = tid_to_last_backtrace_.find(pair.first); it != tid_to_last_backtrace_.end()) {
      if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second) < cooldown_duration_) {
        continue;
      }
    }
    Thread::signalThread(pair.first, SIGUSR2);
    tid_to_last_backtrace_[pair.first] = now;
  }
}

} // namespace BacktraceAction
} // namespace Watchdog
} // namespace Extensions
} // namespace Envoy
