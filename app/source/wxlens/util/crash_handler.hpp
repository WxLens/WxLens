#pragma once

#include <string>

namespace wxlens
{
namespace util
{

/**
 * Installs a process-wide handler that writes a symbolized backtrace to a crash log when the
 * application faults.
 *
 * Motivation (docs/ROADMAP.md, Phase 1 slice 4): a crash reported only as "faulting module
 * Qt6Gui.dll, offset 0x...", with no debugger and no symbols on the machine, is not actionable -
 * the offset cannot be turned into a function name. This makes the process self-reporting instead.
 *
 * Deliberately does **not** use wxlens::log/spdlog: the fault this was built for happens during
 * application teardown, when the logger's sinks may already be destroyed. The handler writes with
 * plain Win32 file calls and touches no allocator or C++ runtime state that teardown could have
 * invalidated.
 *
 * Returns EXCEPTION_CONTINUE_SEARCH after writing, so Windows Error Reporting still produces its
 * usual minidump - this adds a readable stack, it doesn't replace the dump.
 *
 * No-op on non-Windows platforms (the crash log is a DbgHelp/StackWalk64 facility). A POSIX
 * equivalent would use backtrace()/backtrace_symbols() and belongs here if/when WxLens actually
 * builds there.
 */
void InstallCrashHandler(const std::string& crashLogDirectory);

/**
 * Writes a symbolized stack for every thread in this process to the same crash log, tagged with
 * `reason`. Intended for diagnosing hangs, where there is no exception to hook: a deadlocked
 * process produces no crash and no output, so the stacks have to be requested explicitly.
 *
 * Suspends each thread (other than the caller) just long enough to walk it. Safe to call from a
 * watchdog thread; not safe to call from a thread that holds a lock the suspended threads need.
 */
void DumpAllThreadStacks(const char* reason);

/**
 * Starts a detached watchdog that calls DumpAllThreadStacks if the process is still alive
 * `seconds` after this is called. Intended to be armed on QCoreApplication::aboutToQuit so a
 * shutdown that never completes reports where it is stuck instead of just hanging. If the process
 * exits normally first, the watchdog dies with it and writes nothing.
 */
void ArmShutdownWatchdog(int seconds);

} // namespace util
} // namespace wxlens
