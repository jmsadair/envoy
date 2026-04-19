#include <atomic>
#include <csignal>

#include "source/common/signal/non_fatal_signal_action.h"
#include "source/common/signal/non_fatal_signal_handler.h"

#include "gtest/gtest.h"

namespace Envoy {

// Concrete handler that counts invocations using atomics (async-signal-safe).
class CountingSignalHandler : public NonFatalSignalHandlerInterface {
public:
  void onNonFatalSignal(int sig, siginfo_t* /*info*/, void* /*context*/) const override {
    last_sig_.store(sig, std::memory_order_relaxed);
    call_count_.fetch_add(1, std::memory_order_relaxed);
  }

  mutable std::atomic<int> call_count_{0};
  mutable std::atomic<int> last_sig_{0};
};

class NonFatalSignalHandlerTest : public ::testing::Test {
protected:
  void TearDown() override {
    for (auto& h : handlers_) {
      NonFatalSignalHandler::removeNonFatalSignalHandler(*h);
    }
    handlers_.clear();
  }

  CountingSignalHandler& addHandler() {
    handlers_.push_back(std::make_unique<CountingSignalHandler>());
    EXPECT_TRUE(NonFatalSignalHandler::registerNonFatalSignalHandler(*handlers_.back()));
    return *handlers_.back();
  }

  std::vector<std::unique_ptr<CountingSignalHandler>> handlers_;
};

TEST_F(NonFatalSignalHandlerTest, RegisteredHandlerIsCalled) {
  CountingSignalHandler& handler = addHandler();
  NonFatalSignalHandler::callNonFatalSignalHandlers(SIGUSR2, nullptr, nullptr);
  EXPECT_EQ(handler.call_count_, 1);
  EXPECT_EQ(handler.last_sig_, SIGUSR2);
}

TEST_F(NonFatalSignalHandlerTest, RemovedHandlerIsNotCalled) {
  CountingSignalHandler handler;
  EXPECT_TRUE(NonFatalSignalHandler::registerNonFatalSignalHandler(handler));
  NonFatalSignalHandler::removeNonFatalSignalHandler(handler);
  NonFatalSignalHandler::callNonFatalSignalHandlers(SIGUSR2, nullptr, nullptr);
  EXPECT_EQ(handler.call_count_, 0);
}

TEST_F(NonFatalSignalHandlerTest, MultipleHandlersAllCalled) {
  CountingSignalHandler& h1 = addHandler();
  CountingSignalHandler& h2 = addHandler();
  CountingSignalHandler& h3 = addHandler();

  NonFatalSignalHandler::callNonFatalSignalHandlers(SIGUSR2, nullptr, nullptr);
  EXPECT_EQ(h1.call_count_, 1);
  EXPECT_EQ(h2.call_count_, 1);
  EXPECT_EQ(h3.call_count_, 1);
}

TEST_F(NonFatalSignalHandlerTest, MaxHandlersExceededReturnsFalse) {
  for (int i = 0; i < 16; i++) {
    addHandler();
  }
  CountingSignalHandler overflow;
  EXPECT_FALSE(NonFatalSignalHandler::registerNonFatalSignalHandler(overflow));
}

TEST_F(NonFatalSignalHandlerTest, OnlyRemovedHandlerIsSkipped) {
  CountingSignalHandler& h1 = addHandler();
  CountingSignalHandler h2;
  EXPECT_TRUE(NonFatalSignalHandler::registerNonFatalSignalHandler(h2));
  CountingSignalHandler& h3 = addHandler();

  NonFatalSignalHandler::removeNonFatalSignalHandler(h2);
  NonFatalSignalHandler::callNonFatalSignalHandlers(SIGUSR2, nullptr, nullptr);

  EXPECT_EQ(h1.call_count_, 1);
  EXPECT_EQ(h2.call_count_, 0);
  EXPECT_EQ(h3.call_count_, 1);
}

TEST(NonFatalSignalActionTest, InstalledAfterConstructionRemovedAfterDestruction) {
  ASSERT_FALSE(NonFatalSignalAction::isInstalled());
  {
    NonFatalSignalAction action;
    EXPECT_TRUE(NonFatalSignalAction::isInstalled());
  }
  EXPECT_FALSE(NonFatalSignalAction::isInstalled());
}

TEST(NonFatalSignalActionTest, SIGUSR2DispatchesToRegisteredHandlers) {
  CountingSignalHandler handler;
  ASSERT_TRUE(NonFatalSignalHandler::registerNonFatalSignalHandler(handler));

  {
    NonFatalSignalAction action;
    raise(SIGUSR2);
  }

  NonFatalSignalHandler::removeNonFatalSignalHandler(handler);
  EXPECT_GE(handler.call_count_.load(), 1);
  EXPECT_EQ(handler.last_sig_.load(), SIGUSR2);
}

} // namespace Envoy
