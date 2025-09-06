#include "app.h"
#include "core/logger.h"
#if defined(_WIN32)
#  include <windows.h>
#  include <fstream>
static std::wstring GetExeDir() {
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring p(path);
  size_t pos = p.find_last_of(L"/\\");
  if (pos != std::wstring::npos) return p.substr(0, pos);
  return L".";
}
static void WriteCrashLog(const char* msg) {
  try {
    std::wstring dir = GetExeDir();
    std::wstring log = dir + L"\\crash.log";
    std::ofstream f(log, std::ios::app);
    if (f) { f << msg << "\n"; }
  } catch(...) {}
}
static LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS *ep) {
  if (ep && ep->ExceptionRecord) {
    auto code = ep->ExceptionRecord->ExceptionCode;
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    char buf[256];
    snprintf(buf, sizeof(buf), "SEH exception 0x%08lX at %p", (unsigned long)code, addr);
    Core::Logger::instance().error(std::string(buf));
    WriteCrashLog(buf);
  } else {
    Core::Logger::instance().error("SEH exception (no details)");
    WriteCrashLog("SEH exception (no details)");
  }
  Sleep(3000);
  return EXCEPTION_EXECUTE_HANDLER;
}
static LONG NTAPI VectoredHandler(PEXCEPTION_POINTERS ep) {
  if (ep && ep->ExceptionRecord) {
    auto code = ep->ExceptionRecord->ExceptionCode;
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    char buf[256];
    snprintf(buf, sizeof(buf), "VEH exception 0x%08lX at %p", (unsigned long)code, addr);
    WriteCrashLog(buf);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main() {
#if defined(_WIN32)
  SetUnhandledExceptionFilter(TopLevelFilter);
  AddVectoredExceptionHandler(1, VectoredHandler);
#endif
  try {
    App app;
    return app.run();
  } catch (const std::exception &e) {
    Core::Logger::instance().error(std::string("Unhandled std::exception: ") + e.what());
    WriteCrashLog((std::string("Unhandled std::exception: ") + e.what()).c_str());
  } catch (...) {
    Core::Logger::instance().error("Unhandled unknown exception");
    WriteCrashLog("Unhandled unknown exception");
  }
  Sleep(2000);
  return -1;
}

