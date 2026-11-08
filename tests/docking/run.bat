@echo off
setlocal
if not defined MSYS_ROOT set MSYS_ROOT=C:\msys64
set PATH=%MSYS_ROOT%\mingw64\bin;%PATH%
cd /d "%~dp0\..\.."
if not exist build mkdir build
set TP=third_party

g++ -std=gnu++20 -O1 -Isrc -I%TP% -I%TP%\imgui -idirafter %MSYS_ROOT%\ucrt64\include ^
    tests\docking\dock_tests.cpp tests\docking\stubs.cpp src\ui\DockLayout.cpp src\ui\GlassTheme.cpp src\core\UISettings.cpp ^
    %TP%\imgui\imgui.cpp %TP%\imgui\imgui_draw.cpp %TP%\imgui\imgui_tables.cpp %TP%\imgui\imgui_widgets.cpp ^
    -static-libstdc++ -static-libgcc -o build\dock_tests.exe || exit /b 1

for %%f in (tests\docking\layouts\*.ini) do (
    build\dock_tests.exe ini %%f || exit /b 1
)
build\dock_tests.exe %*
