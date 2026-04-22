#pragma once

#include <csignal>

namespace Envoy {

// Function pointer type for non-fatal signal handlers. Implementations must
// be async-signal-safe and must not capture instance state.
using NonFatalSignalCallback = void (*)(int sig, siginfo_t* info, void* context);

namespace NonFatalSignalHandler {

constexpr size_t MaxHandlers = 16;

/**
 * Add this callback to the list of functions called when Envoy receives a
 * non-fatal signal. The callback must be async-signal-safe. Returns true if
 * successfully registered, false if the handler limit has been reached.
 */
bool registerNonFatalSignalHandler(NonFatalSignalCallback cb);

/**
 * Remove this callback from the list if it exists.
 */
void removeNonFatalSignalHandler(NonFatalSignalCallback cb);

/**
 * Call all registered handlers. Async-signal-safe; intended to be called
 * from a signal handler.
 */
void callNonFatalSignalHandlers(int sig, siginfo_t* info, void* context);

} // namespace NonFatalSignalHandler
} // namespace Envoy
