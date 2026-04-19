#pragma once

#include <csignal>

#include "envoy/common/pure.h"

namespace Envoy {

// A simple class which allows registering functions to be called when Envoy
// receives a non-fatal signal, documented in non_fatal_signal_action.h.
class NonFatalSignalHandlerInterface {
public:
  virtual ~NonFatalSignalHandlerInterface() = default;
  // Called when Envoy receives a non-fatal signal. Must be async-signal-safe.
  virtual void onNonFatalSignal(int sig, siginfo_t* info, void* context) const PURE;
};

namespace NonFatalSignalHandler {
/**
 * Add this handler to the list of functions which will be called if Envoy
 * receives a non-fatal signal. Returns true if the the handler was successfully
 * registered and false otherwise.
 */
bool registerNonFatalSignalHandler(const NonFatalSignalHandlerInterface& handler);

/**
 * Removes this handler from the list of functions which will be called if Envoy
 * receives a non-fatal signal.
 */
void removeNonFatalSignalHandler(const NonFatalSignalHandlerInterface& handler);

/**
 * Calls the signal handlers registered with registerNonFatalSignalHandler.
 * This is async-signal-safe and intended to be called from a signal handler.
 */
void callNonFatalSignalHandlers(int sig, siginfo_t* info, void* context);

} // namespace NonFatalSignalHandler
} // namespace Envoy
