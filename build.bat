@echo off
REM ============================================================
REM  Build the Bridge Commander community-patch proxy DLL (x86).
REM  Output: dist\binkw32.dll
REM ============================================================
setlocal

set VCVARS="C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
  echo [!] vcvars32.bat not found at %VCVARS%
  echo     Edit build.bat and point VCVARS at your x86 Build Tools.
  exit /b 1
)
call %VCVARS% >nul

set ROOT=%~dp0
set SRC=%ROOT%src
set OUT=%ROOT%dist
set OBJ=%ROOT%build
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

REM Refresh the export forwarders from the original binkw32.dll.
set ORIG=%SRC%\binkw32_orig_ref.dll
if exist "%ORIG%" (
  echo [*] Regenerating export forwarders from %ORIG%
  python "%ROOT%tools\gen_forwards.py" "%ORIG%" "%SRC%\exports_generated.h"
) else (
  echo [*] Using existing exports_generated.h (no binkw32_orig_ref.dll present)
)

echo [*] Compiling...
cl /nologo /O2 /MT /EHsc /GS /W3 /DWIN32 /D_WINDOWS ^
   /Fo"%OBJ%\\" /Fd"%OBJ%\\" ^
   "%SRC%\dllmain.cpp" "%SRC%\fix_crashreporter.cpp" "%SRC%\pyfix_none.cpp" ^
   /LD /Fe:"%OUT%\binkw32.dll" ^
   /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT ^
   user32.lib

if %ERRORLEVEL% neq 0 (
  echo [!] Build failed.
  exit /b 1
)
echo.
echo [+] Built %OUT%\binkw32.dll
echo     Install: rename the game's binkw32.dll to binkw32_orig.dll,
echo     then copy this binkw32.dll into the game folder.
endlocal
