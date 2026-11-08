@echo off
setlocal
if not defined MSYS_ROOT set MSYS_ROOT=C:\msys64
set PATH=%MSYS_ROOT%\mingw64\bin;%PATH%
cd /d "%~dp0\..\.."
if not exist build mkdir build
g++ -std=gnu++20 -O1 -Isrc -Ithird_party -Ithird_party\glad\include -idirafter %MSYS_ROOT%\ucrt64\include ^
    tests\export\export_tests.cpp src\render\ModelExport.cpp -L%MSYS_ROOT%\ucrt64\lib -lassimp -l:libz.a -static-libstdc++ -static-libgcc -o build\export_tests.exe || exit /b 1
set PATH=%CD%\bin;%PATH%
build\export_tests.exe
