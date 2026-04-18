#pragma once

#include "envoy/thread/thread.h"

namespace Envoy {
namespace Thread {
/**
 * Tries to terminates the process by killing the thread specified by
 * the ThreadId. The implementation is platform dependent and currently
 * only works on platforms that support signals.
 *
 * Returns true if the platform specific function to terminate the thread
 * succeeded (i.e. kill() == 0). If the platform is currently unsupported, this
 * will return false.
 */
bool terminateThread(const ThreadId& tid);

/**
 * Tries to send the provided signal to the specified ThreadId. The
 * implementation is platform dependent and currently only works on
 * platforms that support signals.
 *
 * Returns true if the platform specific function to signal the thread
 * succeeded (i.e. kill() == 0). If the platform is currently unsupported,
 * this will return false.
 */
bool signalThread(const ThreadId& tid, int signal);
} // namespace Thread
} // namespace Envoy
