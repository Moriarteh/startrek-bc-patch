// See fix_crashreporter.h. A crash-safe drop-in for the fault-address resolver.

#include <windows.h>

#include "fix_crashreporter.h"

namespace crashfix {
namespace {

constexpr DWORD_PTR kResolver = 0x007170D0;  // the fault-address resolver (the unsafe one)

// Expected prologue: sub esp,0x1C ; lea eax,[esp] ; (push ebx...)
const BYTE kExpected[7] = {0x83, 0xEC, 0x1C, 0x8D, 0x44, 0x24, 0x00};

void PatchLog(const char* fmt, ...) {
  char dir[MAX_PATH];
  GetModuleFileNameA(nullptr, dir, MAX_PATH);
  char* slash = dir;
  for (char* p = dir; *p; ++p)
    if (*p == '\\' || *p == '/') slash = p;
  *slash = 0;
  lstrcatA(dir, "\\crashlogs");
  CreateDirectoryA(dir, nullptr);
  lstrcatA(dir, "\\patch.log");
  HANDLE h = CreateFileA(dir, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  SetFilePointer(h, 0, nullptr, FILE_END);
  SYSTEMTIME st;
  GetLocalTime(&st);
  char body[512];
  va_list ap;
  va_start(ap, fmt);
  wvsprintfA(body, fmt, ap);
  va_end(ap);
  char line[640];
  wsprintfA(line, "[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n", st.wYear, st.wMonth,
            st.wDay, st.wHour, st.wMinute, st.wSecond, body);
  DWORD n;
  WriteFile(h, line, lstrlenA(line), &n, nullptr);
  CloseHandle(h);
}

// Faithful, crash-safe reimplementation of the fault-address resolver.
//   in:  addr         - a code address (e.g. the faulting EIP)
//        nameBuf/size - receives the owning module's file name
//   out: *outSection  - 1-based PE section index (0 if unresolved)
//        *outOffset   - offset of addr within that section (or 0)
//   ret: 1 if resolved, 0 otherwise.
// The original did the same by hand but never checked the module base, so a
// null address walked it straight into the null page. This version validates
// everything and wraps the parse in a structured-exception guard.
int __cdecl SafeResolve(const void* addr, char* nameBuf, DWORD bufSize,
                        int* outSection, int* outOffset) {
  if (nameBuf && bufSize) nameBuf[0] = 0;
  if (outSection) *outSection = 0;
  if (outOffset) *outOffset = 0;

  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) return 0;
  BYTE* base = reinterpret_cast<BYTE*>(mbi.AllocationBase);
  if (!base) return 0;                 // the fix: no module here (null/garbage)
  if (mbi.State != MEM_COMMIT) return 0;

  __try {
    if (GetModuleFileNameA((HMODULE)base, nameBuf, bufSize) == 0) {
      if (nameBuf && bufSize) nameBuf[0] = 0;
      return 0;
    }
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    DWORD rva = (DWORD)((const BYTE*)addr - base);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    int count = nt->FileHeader.NumberOfSections;
    for (int i = 0; i < count; ++i) {
      DWORD va = sec[i].VirtualAddress;
      DWORD span = sec[i].Misc.VirtualSize;      // match original: max(virt,raw)
      if (span < sec[i].SizeOfRawData) span = sec[i].SizeOfRawData;
      if (rva >= va && rva <= va + span) {
        if (outSection) *outSection = i + 1;
        if (outOffset) *outOffset = (int)(rva - va);
        return 1;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;  // anything weird about this "module" -> just report unresolved
  }
  return 0;
}

}  // namespace

void Install() {
  BYTE* p = reinterpret_cast<BYTE*>(kResolver);
  for (int i = 0; i < 7; ++i)
    if (p[i] != kExpected[i]) {
      PatchLog("crashreporter fix SKIPPED: the fault-address resolver prologue mismatch "
               "(unexpected stbc.exe build?)");
      return;
    }

  DWORD old;
  if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) {
    p[0] = 0xE9;  // jmp rel32 -> SafeResolve (full replacement; never returns)
    *reinterpret_cast<DWORD*>(p + 1) =
        (DWORD)&SafeResolve - (DWORD)(kResolver + 5);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    PatchLog("crashreporter fix applied: the fault-address resolver made crash-safe");
  } else {
    PatchLog("crashreporter fix FAILED: VirtualProtect error");
  }
}

}  // namespace crashfix
