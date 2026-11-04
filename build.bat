@echo off
rem MiniTime local build script (llvm-mingw)
rem Compiles all modular sources plus resources into ONE single exe.
setlocal

set "MINGW=D:\windows\Desktop\dsh本地工作目录\llvm-mingw-20261020-msvcrt-i686"
if not exist "%MINGW%\bin\x86_64-w64-mingw32-g++.exe" (
    echo [ERROR] llvm-mingw not found: %MINGW%
    echo Please edit MINGW path in this script
    exit /b 1
)

if not exist build mkdir build

echo [1/3] compiling resources...
"%MINGW%\bin\x86_64-w64-mingw32-windres.exe" --target=pe-x86-64 -I res -I src res\resource.rc -O coff -o build\resource.res
if errorlevel 1 (
    echo [FAILED] resource compile error
    exit /b 1
)

echo [2/3] compiling sources...
"%MINGW%\bin\x86_64-w64-mingw32-g++.exe" ^
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