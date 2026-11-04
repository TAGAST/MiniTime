@echo off
rem MiniTime local build script (llvm-mingw)
rem Compiles all modular sources plus resources into ONE single exe.
rem
rem Requirements:
rem   llvm-mingw (https://github.com/mstorsjo/llvm-mingw/releases)
rem
rem Resolved in this order:
rem   1. MINGW environment variable pointing at the llvm-mingw root
rem   2. llvm-mingw placed next to this repository as ..\llvm-mingw
rem   3. x86_64-w64-mingw32-g++.exe already on PATH
rem   4. common install locations
setlocal

cd /d "%~dp0"

set "GPP="
set "WINDRES="

rem --- 1) explicit env var ---------------------------------------------
if defined MINGW (
    if exist "%MINGW%\bin\x86_64-w64-mingw32-g++.exe" (
        set "GPP=%MINGW%\bin\x86_64-w64-mingw32-g++.exe"
        set "WINDRES=%MINGW%\bin\x86_64-w64-mingw32-windres.exe"
    )
)

rem --- 2) sibling directory -------------------------------------------
if not defined GPP (
    if exist "%~dp0..\llvm-mingw\bin\x86_64-w64-mingw32-g++.exe" (
        set "GPP=%~dp0..\llvm-mingw\bin\x86_64-w64-mingw32-g++.exe"
        set "WINDRES=%~dp0..\llvm-mingw\bin\x86_64-w64-mingw32-windres.exe"
    )
)

rem --- 3) already on PATH ----------------------------------------------
if not defined GPP (
    for /f "delims=" %%I in ('where x86_64-w64-mingw32-g++.exe 2^>nul') do (
        if not defined GPP set "GPP=%%I"
    )
    if not defined GPP (
        for /f "delims=" %%I in ('where g++ 2^>nul') do (
            if not defined GPP (
                if exist "%%~dpIx86_64-w64-mingw32-windres.exe" (
                    set "GPP=%%I"
                    set "WINDRES=%%~dpIx86_64-w64-mingw32-windres.exe"
                )
            )
        )
    )
    if defined GPP if not defined WINDRES (
        for /f "delims=" %%I in ('where x86_64-w64-mingw32-windres.exe 2^>nul') do (
            if not defined WINDRES set "WINDRES=%%I"
        )
    )
)

rem --- 4) common locations --------------------------------------------
if not defined GPP (
    for %%D in (
        "%ProgramFiles%\llvm-mingw"
        "%LOCALAPPDATA%\llvm-mingw"
        "C:\llvm-mingw"
        "D:\tools\llvm-mingw"
    ) do (
        if not defined GPP if exist "%%~D\bin\x86_64-w64-mingw32-g++.exe" (
            set "GPP=%%~D\bin\x86_64-w64-mingw32-g++.exe"
            set "WINDRES=%%~D\bin\x86_64-w64-mingw32-windres.exe"
        )
    )
)

if not defined GPP (
    echo [ERROR] llvm-mingw not found.
    echo.
    echo Do ONE of the following, then re-run build.bat:
    echo   - set MINGW to the llvm-mingw root directory
    echo   - place llvm-mingw at ..\llvm-mingw next to this repository
    echo   - add llvm-mingw\bin to PATH
    echo.
    echo Download: https://github.com/mstorsjo/llvm-mingw/releases
    exit /b 1
)

if not defined WINDRES set "WINDRES=x86_64-w64-mingw32-windres.exe"

echo Compiler: %GPP%

if not exist build mkdir build

echo [1/3] compiling resources...
"%WINDRES%" --target=pe-x86-64 -I res -I src res\resource.rc -O coff -o build\resource.res
if errorlevel 1 (
    echo [FAILED] resource compile error
    exit /b 1
)

echo [2/3] compiling sources...
"%GPP%" ^
  -std=c++20 -municode -mwindows ^
  -Os -flto -fno-exceptions -fno-rtti ^
  -ffunction-sections -fdata-sections ^
  -Wall -Wextra ^
  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
  -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 ^
  -Isrc -Ires ^
  src\*.cpp ^
  build\resource.res ^
  -o build\MiniTime.exe ^
  -Wl,--gc-sections -Wl,-s -static-libgcc -static-libstdc++ ^
  -luser32 -lgdi32 -lshell32 -ladvapi32 -lole32 -lcomctl32 -lshcore

if errorlevel 1 (
    echo [FAILED] build error
    exit /b 1
)

echo [3/3] done
for %%F in (build\MiniTime.exe) do set SIZE=%%~zF
echo [OK] build\MiniTime.exe  %SIZE% bytes
endlocal
