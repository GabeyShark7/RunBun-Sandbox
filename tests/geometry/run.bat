@echo off
setlocal
if not defined MSYS_ROOT set MSYS_ROOT=C:\msys64
set PATH=%MSYS_ROOT%\mingw64\bin;%PATH%
cd /d "%~dp0\..\.."
if not exist build mkdir build
g++ -std=gnu++20 -O1 -Isrc -Ithird_party -Ithird_party\glad\include -idirafter %MSYS_ROOT%\ucrt64\include ^
    tests\geometry\geo_tests.cpp src\render\PolyMesh.cpp src\render\UVUnwrap.cpp src\render\CSG.cpp src\render\Modifiers.cpp src\render\EditMesh.cpp src\render\Primitives.cpp tests\geometry\stubs.cpp -static-libstdc++ -static-libgcc -o build\geo_tests.exe || exit /b 1
build\geo_tests.exe
