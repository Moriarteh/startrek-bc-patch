// See pyfix_none.h. The second bug fix: keeping None alive forever.
//
// Background, for the curious: Bridge Commander embeds Python 1.5.2. Every
// Python value is reference-counted: Py_INCREF to add a reference,
// Py_DECREF to drop one, and when the count hits zero the object's
// type->tp_dealloc is called to free it. None is a single shared, immortal
// object, so its type's tp_dealloc was left NULL ("never called", says the
// CPython source). If some code path does one too many Py_DECREFs on None, the
// count eventually reaches zero, Py_DECREF calls that NULL pointer, and EIP=0.
//
// A 50v50 battle runs the ship-AI scripts a *lot*, so even a tiny per-frame
// leak drains None's refcount in minutes. Rather than hunt every unbalanced
// DECREF across the game's compiled scripts, this fix does what CPython itself
// later did for None/True/False: make it immortal.

#include <windows.h>

#include "pyfix_none.h"

namespace pyfix {
namespace {

// --- Addresses in the GOG retail stbc.exe (ImageBase 0x400000, non-ASLR). ---
// Object layout is standard Python 1.5.2:
//   _Py_NoneStruct : { int ob_refcnt; PyTypeObject* ob_type; }
//   PyNothing_Type : { ...; char* tp_name@+0x0C; ...; destructor tp_dealloc@+0x18; }
constexpr DWORD_PTR kPyNone     = 0x0095DD20;  // &_Py_NoneStruct
constexpr DWORD_PTR kNoneType   = 0x0095DCB0;  // &PyNothing_Type
constexpr DWORD_PTR kTpNameOff  = 0x0C;        // PyTypeObject::tp_name
constexpr DWORD_PTR kTpDealloc  = 0x18;        // PyTypeObject::tp_dealloc

// A big, comfortably-in-the-middle refcount: ~1.07 billion net DECREFs away
// from zero in either direction. None never legitimately gets freed, so this
// is purely a floor that the leak can nibble at harmlessly.
constexpr LONG kImmortal = 0x40000000;

void PatchLog(const char* msg) {
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
  DWORD n;
  SYSTEMTIME st;
  GetLocalTime(&st);
  char line[512];
  wsprintfA(line, "[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n", st.wYear, st.wMonth,
            st.wDay, st.wHour, st.wMinute, st.wSecond, msg);
  WriteFile(h, line, lstrlenA(line), &n, nullptr);
  CloseHandle(h);
}

// A safety net. If None's refcount somehow still reaches zero (it shouldn't),
// Python calls this instead of a NULL pointer, and it simply brings None back
// to life rather than "freeing" a static object. __cdecl to match how the
// interpreter calls tp_dealloc (verified: caller does `add esp,4`).
void __cdecl NoneRevive(void* self) {
  if (self) *reinterpret_cast<volatile LONG*>(self) = kImmortal;
}

template <typename T>
bool Poke(DWORD_PTR addr, T value) {
  DWORD old;
  if (!VirtualProtect((LPVOID)addr, sizeof(T), PAGE_READWRITE, &old))
    return false;
  *reinterpret_cast<T*>(addr) = value;
  VirtualProtect((LPVOID)addr, sizeof(T), old, &old);
  return true;
}

}  // namespace

bool ImmortalizeNone() {
  // Signature check: confirm this really is Py_None on this build before
  // poking memory. ob_type must point at the None type, and
  // that type must be named "None".
  __try {
    DWORD_PTR obType = *reinterpret_cast<DWORD_PTR*>(kPyNone + 4);
    if (obType != kNoneType) {
      PatchLog("None fix SKIPPED: ob_type mismatch (unexpected stbc.exe build?)");
      return false;
    }
    const char* name =
        *reinterpret_cast<const char**>(kNoneType + kTpNameOff);
    if (!name || name[0] != 'N' || name[1] != 'o' || name[2] != 'n' ||
        name[3] != 'e' || name[4] != 0) {
      PatchLog("None fix SKIPPED: tp_name is not \"None\" (unexpected build?)");
      return false;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    PatchLog("None fix SKIPPED: could not read Py_None (unexpected build?)");
    return false;
  }

  bool ok = Poke<LONG>(kPyNone, kImmortal);           // pin the refcount
  ok &= Poke<void*>(kNoneType + kTpDealloc, &NoneRevive);  // backstop dealloc
  PatchLog(ok ? "None fix applied: Py_None immortalized (big-battle crash)"
              : "None fix FAILED: VirtualProtect/write error");
  return ok;
}

}  // namespace pyfix
