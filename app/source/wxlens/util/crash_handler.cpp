#include <wxlens/util/crash_handler.hpp>

#if defined(_WIN32)

// <windows.h> must precede <dbghelp.h>, and NOMINMAX/WIN32_LEAN_AND_MEAN keep it from leaking
// min/max macros into anything that includes this translation unit's headers later (see
// app/CMakeLists.txt's NOMINMAX comment for the failure mode that causes).
#   ifndef NOMINMAX
#      define NOMINMAX
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>

#   include <dbghelp.h>
#   include <tlhelp32.h>

#   include <cstdio>
#   include <thread>

namespace wxlens
{
namespace util
{

namespace
{

// Everything below runs *inside* an exception handler, potentially during teardown or on a
// corrupted heap, so it deliberately avoids std::string, iostreams, and any allocation. Fixed
// buffers and Win32 calls only - a crash handler that itself crashes reports nothing.

char g_crashLogPath[MAX_PATH] {};

void WriteAll(HANDLE file, const char* text)
{
   if (file == INVALID_HANDLE_VALUE || text == nullptr)
   {
      return;
   }

   DWORD  written = 0;
   size_t length  = 0;
   while (text[length] != '\0')
   {
      ++length;
   }
   WriteFile(file, text, static_cast<DWORD>(length), &written, nullptr);
}

const char* ExceptionCodeName(DWORD code)
{
   switch (code)
   {
   case EXCEPTION_ACCESS_VIOLATION:
      return "EXCEPTION_ACCESS_VIOLATION";
   case EXCEPTION_STACK_OVERFLOW:
      return "EXCEPTION_STACK_OVERFLOW";
   case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "EXCEPTION_ILLEGAL_INSTRUCTION";
   case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "EXCEPTION_INT_DIVIDE_BY_ZERO";
   case EXCEPTION_PRIV_INSTRUCTION:
      return "EXCEPTION_PRIV_INSTRUCTION";
   case EXCEPTION_IN_PAGE_ERROR:
      return "EXCEPTION_IN_PAGE_ERROR";
   case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "EXCEPTION_DATATYPE_MISALIGNMENT";
   default:
      return "unknown exception";
   }
}

void WriteFrame(HANDLE file, int index, DWORD64 address)
{
   char line[1024] {};

   // SYMBOL_INFO is variable-length: the name is written past the end of the struct, so it has to
   // live in a buffer sized struct + MAX_SYM_NAME.
   alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)] {};
   auto* symbol        = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
   symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
   symbol->MaxNameLen   = MAX_SYM_NAME;

   const HANDLE process = GetCurrentProcess();

   char        moduleName[MAX_PATH] {};
   const char* shortModule = "?";
   DWORD64     moduleBase  = SymGetModuleBase64(process, address);
   if (moduleBase != 0 &&
       GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), moduleName, MAX_PATH) != 0)
   {
      shortModule = moduleName;
      for (const char* c = moduleName; *c != '\0'; ++c)
      {
         if (*c == '\\' || *c == '/')
         {
            shortModule = c + 1;
         }
      }
   }

   DWORD64 displacement = 0;
   if (SymFromAddr(process, address, &displacement, symbol))
   {
      DWORD           lineDisplacement = 0;
      IMAGEHLP_LINE64 lineInfo {};
      lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

      if (SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo))
      {
         _snprintf_s(line,
                     sizeof(line),
                     _TRUNCATE,
                     "  [%02d] %s!%s + 0x%llx\r\n       %s:%lu\r\n",
                     index,
                     shortModule,
                     symbol->Name,
                     static_cast<unsigned long long>(displacement),
                     lineInfo.FileName,
                     static_cast<unsigned long>(lineInfo.LineNumber));
      }
      else
      {
         _snprintf_s(line,
                     sizeof(line),
                     _TRUNCATE,
                     "  [%02d] %s!%s + 0x%llx\r\n",
                     index,
                     shortModule,
                     symbol->Name,
                     static_cast<unsigned long long>(displacement));
      }
   }
   else
   {
      // No symbols for this module (e.g. a system DLL, or Qt without PDBs). Module + offset is
      // still useful: it is exactly what the Windows event log reports, and pins the module.
      _snprintf_s(line,
                  sizeof(line),
                  _TRUNCATE,
                  "  [%02d] %s + 0x%llx  (no symbols)\r\n",
                  index,
                  shortModule,
                  static_cast<unsigned long long>(address - moduleBase));
   }

   WriteAll(file, line);
}

// Walks `thread` using `context` and writes its frames. Shared by the exception handler and the
// all-threads dump so both produce identical output.
void WalkAndWriteStack(HANDLE file, HANDLE thread, CONTEXT context)
{
   const HANDLE process = GetCurrentProcess();

   STACKFRAME64 frame {};
   frame.AddrPC.Mode    = AddrModeFlat;
   frame.AddrFrame.Mode = AddrModeFlat;
   frame.AddrStack.Mode = AddrModeFlat;
#   if defined(_M_X64)
   const DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
   frame.AddrPC.Offset     = context.Rip;
   frame.AddrFrame.Offset  = context.Rbp;
   frame.AddrStack.Offset  = context.Rsp;
#   elif defined(_M_ARM64)
   const DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
   frame.AddrPC.Offset     = context.Pc;
   frame.AddrFrame.Offset  = context.Fp;
   frame.AddrStack.Offset  = context.Sp;
#   endif

   for (int i = 0; i < 64; ++i)
   {
      if (!StackWalk64(machineType,
                       process,
                       thread,
                       &frame,
                       &context,
                       nullptr,
                       SymFunctionTableAccess64,
                       SymGetModuleBase64,
                       nullptr) ||
          frame.AddrPC.Offset == 0)
      {
         break;
      }

      WriteFrame(file, i, frame.AddrPC.Offset);
   }
}

LONG WINAPI HandleException(EXCEPTION_POINTERS* exceptionInfo)
{
   const HANDLE file = CreateFileA(g_crashLogPath,
                                   FILE_APPEND_DATA,
                                   FILE_SHARE_READ,
                                   nullptr,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
   if (file == INVALID_HANDLE_VALUE)
   {
      return EXCEPTION_CONTINUE_SEARCH;
   }

   char header[512] {};
   SYSTEMTIME now {};
   GetLocalTime(&now);

   const DWORD   code    = exceptionInfo->ExceptionRecord->ExceptionCode;
   const DWORD64 address = reinterpret_cast<DWORD64>(exceptionInfo->ExceptionRecord->ExceptionAddress);

   _snprintf_s(header,
               sizeof(header),
               _TRUNCATE,
               "\r\n=== wxlens-app crash %04u-%02u-%02u %02u:%02u:%02u ===\r\n"
               "exception : 0x%08lx (%s)\r\n"
               "address   : 0x%llx\r\n"
               "thread    : %lu\r\n",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
               static_cast<unsigned long>(code),
               ExceptionCodeName(code),
               static_cast<unsigned long long>(address),
               static_cast<unsigned long>(GetCurrentThreadId()));
   WriteAll(file, header);

   if (code == EXCEPTION_ACCESS_VIOLATION &&
       exceptionInfo->ExceptionRecord->NumberParameters >= 2)
   {
      char access[256] {};
      const ULONG_PTR operation = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
      const ULONG_PTR faultAddr = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
      _snprintf_s(access,
                  sizeof(access),
                  _TRUNCATE,
                  "access    : %s 0x%llx\r\n",
                  operation == 0 ? "read from" : (operation == 1 ? "write to" : "execute at"),
                  static_cast<unsigned long long>(faultAddr));
      WriteAll(file, access);
   }

   WriteAll(file, "stack:\r\n");

   const HANDLE process = GetCurrentProcess();
   const HANDLE thread  = GetCurrentThread();

   SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
   SymInitialize(process, nullptr, TRUE);

   // StackWalk64 mutates the CONTEXT it is given, so walk a copy - the original belongs to the
   // exception record and WER still needs it intact for the minidump.
   WalkAndWriteStack(file, thread, *exceptionInfo->ContextRecord);

   WriteAll(file, "=== end crash ===\r\n");

   SymCleanup(process);
   CloseHandle(file);

   // Let WER still run, so its minidump is written as usual.
   return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void DumpAllThreadStacks(const char* reason)
{
   const HANDLE file = CreateFileA(g_crashLogPath,
                                   FILE_APPEND_DATA,
                                   FILE_SHARE_READ,
                                   nullptr,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
   if (file == INVALID_HANDLE_VALUE)
   {
      return;
   }

   char header[512] {};
   SYSTEMTIME now {};
   GetLocalTime(&now);
   _snprintf_s(header,
               sizeof(header),
               _TRUNCATE,
               "\r\n=== wxlens-app thread dump %04u-%02u-%02u %02u:%02u:%02u ===\r\n"
               "reason: %s\r\n",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
               reason != nullptr ? reason : "(none)");
   WriteAll(file, header);

   const HANDLE process   = GetCurrentProcess();
   const DWORD  processId = GetCurrentProcessId();
   const DWORD  selfId    = GetCurrentThreadId();

   SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
   SymInitialize(process, nullptr, TRUE);

   const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
   if (snapshot != INVALID_HANDLE_VALUE)
   {
      THREADENTRY32 entry {};
      entry.dwSize = sizeof(entry);

      if (Thread32First(snapshot, &entry))
      {
         do
         {
            if (entry.th32OwnerProcessID != processId || entry.th32ThreadID == selfId)
            {
               continue;
            }

            const HANDLE thread = OpenThread(
               THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
               FALSE,
               entry.th32ThreadID);
            if (thread == nullptr)
            {
               continue;
            }

            char threadHeader[128] {};
            _snprintf_s(threadHeader,
                        sizeof(threadHeader),
                        _TRUNCATE,
                        "\r\n-- thread %lu --\r\n",
                        static_cast<unsigned long>(entry.th32ThreadID));
            WriteAll(file, threadHeader);

            if (SuspendThread(thread) != static_cast<DWORD>(-1))
            {
               CONTEXT context {};
               context.ContextFlags = CONTEXT_FULL;
               if (GetThreadContext(thread, &context))
               {
                  WalkAndWriteStack(file, thread, context);
               }
               else
               {
                  WriteAll(file, "  (could not get thread context)\r\n");
               }
               ResumeThread(thread);
            }

            CloseHandle(thread);
         } while (Thread32Next(snapshot, &entry));
      }

      CloseHandle(snapshot);
   }

   WriteAll(file, "=== end thread dump ===\r\n");

   SymCleanup(process);
   CloseHandle(file);
}

void ArmShutdownWatchdog(int seconds)
{
   std::thread(
      [seconds]()
      {
         Sleep(static_cast<DWORD>(seconds) * 1000u);
         // Reaching here means the process outlived its own shutdown budget: a normal exit would
         // have terminated this detached thread long before the sleep elapsed.
         DumpAllThreadStacks("shutdown did not complete within the watchdog interval");
      })
      .detach();
}

void InstallCrashHandler(const std::string& crashLogDirectory)
{
   _snprintf_s(g_crashLogPath,
               sizeof(g_crashLogPath),
               _TRUNCATE,
               "%s\\wxlens-crash.log",
               crashLogDirectory.c_str());

   SetUnhandledExceptionFilter(&HandleException);
}

} // namespace util
} // namespace wxlens

#else

namespace wxlens
{
namespace util
{

void InstallCrashHandler(const std::string&)
{
   // See the header: the Windows implementation is DbgHelp-based. A POSIX backtrace()
   // implementation belongs here when WxLens actually builds on one.
}

void DumpAllThreadStacks(const char*) {}

void ArmShutdownWatchdog(int) {}

} // namespace util
} // namespace wxlens

#endif
