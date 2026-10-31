// Star Trek: Bridge Commander community patch proxy DLL.
//
// This DLL masquerades as binkw32.dll. The real one is renamed binkw32_orig.dll and
// every export is forwarded straight through to it (see exports_generated.h),
// so the game's movie playback is untouched. The actual bug fixes live here.
//
// binkw32 is a static import of stbc.exe, so this DLL loads before the game's
// entry point runs, the right moment to install the fixes.

#include <windows.h>

#include "fix_crashreporter.h"
#include "pyfix_none.h"
#include "exports_generated.h"  // /EXPORT forwarders -> binkw32_orig.dll

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);

    // Start each launch with a fresh status log so it never grows over time.
    // (The fixes below append their one-line status to crashlogs\patch.log.)
    {
      char path[MAX_PATH];
      GetModuleFileNameA(nullptr, path, MAX_PATH);
      char* slash = path;
      for (char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/') slash = p;
      *slash = 0;
      lstrcatA(path, "\\crashlogs");
      CreateDirectoryA(path, nullptr);
      lstrcatA(path, "\\patch.log");
      HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }

    // Fix #1: repair the game's OWN crash reporter (so no separate one is needed).
    crashfix::Install();
    // Fix #2 (big-battle crash): make Py_None immortal so a script-side
    // refcount leak can't drive it to zero and call its NULL destructor.
    pyfix::ImmortalizeNone();
  }
  return TRUE;
}
