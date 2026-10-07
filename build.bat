@echo off
setlocal
cd /d "%~dp0"
if not exist vendor\sqlite3.c (
 echo SQLite source missing. Run powershell -ExecutionPolicy Bypass -File tools\fetch-sqlite.ps1
 exit /b 1
)
if not exist build mkdir build
where cl >nul 2>nul
if not errorlevel 1 goto msvc
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if defined VSROOT call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if not errorlevel 1 goto msvc
for /d %%i in (tools\llvm-mingw*) do if exist "%%i\bin\clang.exe" set "LLVM=%%~fi\bin"
if defined LLVM goto llvm
echo No C compiler found. Install Visual Studio Desktop development with C++, or extract LLVM-MinGW under tools.
exit /b 1
:msvc
set "COMMON=/nologo /O1 /W4 /std:c11 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /Isrc /Ivendor"
cl /nologo /O1 /w /c /DSQLITE_THREADSAFE=0 /DSQLITE_DEFAULT_MEMSTATUS=0 /DSQLITE_OMIT_LOAD_EXTENSION /DSQLITE_DQS=0 vendor\sqlite3.c /Fobuild\sqlite3.obj
if errorlevel 1 exit /b 1
rc /nologo /I src /fo build\netpulse.res src\netpulse.rc
if errorlevel 1 exit /b 1
cl %COMMON% src\main.c src\core.c src\network.c src\db.c src\settings.c src\ui.c src\taskbar.c src\startup.c build\sqlite3.obj build\netpulse.res /Fobuild\ /Febuild\NetPulse.exe /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF user32.lib gdi32.lib shell32.lib advapi32.lib iphlpapi.lib comdlg32.lib dwmapi.lib comctl32.lib ole32.lib oleaut32.lib uuid.lib
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_core.c src\core.c /Fobuild\ /Febuild\test_core.exe
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_db.c src\db.c build\sqlite3.obj /Fobuild\ /Febuild\test_db.exe
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_network.c src\network.c src\core.c /Fobuild\ /Febuild\test_network.exe /link iphlpapi.lib
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_startup.c src\startup.c /Fobuild\ /Febuild\test_startup.exe /link advapi32.lib
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_settings.c src\settings.c src\core.c /Fobuild\ /Febuild\test_settings.exe /link advapi32.lib
if errorlevel 1 exit /b 1
cl %COMMON% tests\test_placement.c /Fobuild\ /Febuild\test_placement.exe
if errorlevel 1 exit /b 1
goto test
:llvm
set "COMMON=-Os -std=c11 -Wall -Wextra -Wno-misleading-indentation -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Isrc -Ivendor"
"%LLVM%\clang.exe" -Os -w -c -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 vendor\sqlite3.c -o build\sqlite3.o
if errorlevel 1 exit /b 1
"%LLVM%\llvm-windres.exe" -I src src\netpulse.rc -O coff -o build\netpulse.res
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% -municode -mwindows src\main.c src\core.c src\network.c src\db.c src\settings.c src\ui.c src\taskbar.c src\startup.c build\sqlite3.o build\netpulse.res -o build\NetPulse.exe -Wl,--gc-sections -s -luser32 -lgdi32 -lshell32 -ladvapi32 -liphlpapi -lcomdlg32 -ldwmapi -lcomctl32 -lole32 -loleaut32 -luuid
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_core.c src\core.c -o build\test_core.exe
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_db.c src\db.c build\sqlite3.o -o build\test_db.exe
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_network.c src\network.c src\core.c -o build\test_network.exe -liphlpapi
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_startup.c src\startup.c -o build\test_startup.exe -ladvapi32
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_settings.c src\settings.c src\core.c -o build\test_settings.exe -ladvapi32
if errorlevel 1 exit /b 1
"%LLVM%\clang.exe" %COMMON% tests\test_placement.c -o build\test_placement.exe
if errorlevel 1 exit /b 1
:test
build\test_core.exe
if errorlevel 1 exit /b 1
build\test_db.exe
if errorlevel 1 exit /b 1
build\test_network.exe
if errorlevel 1 exit /b 1
build\test_settings.exe
if errorlevel 1 exit /b 1
build\test_placement.exe
if errorlevel 1 exit /b 1
copy /y config.ini build\config.ini >nul
echo Built and tested build\NetPulse.exe
