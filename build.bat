@echo off
setlocal

if not defined MSYS_ROOT set MSYS_ROOT=C:\msys64
set MSYS=%MSYS_ROOT%
if not defined RUNBUN_BUILD set RUNBUN_BUILD=0
set PATH=%MSYS%\mingw64\bin;%USERPROFILE%\.cargo\bin;%PATH%
set TP=third_party
set OBJ=build
set OUT=bin
set NET=networking

cd /d "%~dp0"
if not exist %OBJ% mkdir %OBJ%
if not exist %OUT% mkdir %OUT%

> %OBJ%\BuildInfo.h echo #pragma once
>> %OBJ%\BuildInfo.h echo #define RUNBUN_BUILD %RUNBUN_BUILD%
>> %OBJ%\BuildInfo.h echo #define RUNBUN_UPDATE_TOKEN "%RUNBUN_UPDATE_TOKEN%"

cargo +stable-x86_64-pc-windows-gnu build --release --lib --manifest-path %NET%\Cargo.toml || goto fail

gcc -O2 -I%TP%\glad\include -c %TP%\glad\src\glad.c -o %OBJ%\glad.o || goto fail
gcc -O2 -c %TP%\tinyfiledialogs\tinyfiledialogs.c -o %OBJ%\tinyfiledialogs.o || goto fail

g++ -std=gnu++20 -O2 ^
    -Isrc -I%OBJ% -I%TP% -I%TP%\imgui -I%TP%\glad\include -I%NET%\include -idirafter %MSYS%\ucrt64\include ^
    src\*.cpp src\core\*.cpp src\render\*.cpp src\ui\*.cpp ^
    %TP%\imgui\imgui.cpp %TP%\imgui\imgui_draw.cpp %TP%\imgui\imgui_tables.cpp %TP%\imgui\imgui_widgets.cpp %TP%\imgui\imgui_demo.cpp ^
    %TP%\imgui\backends\imgui_impl_glfw.cpp %TP%\imgui\backends\imgui_impl_opengl3.cpp ^
    %OBJ%\glad.o %OBJ%\tinyfiledialogs.o ^
    -L%MSYS%\ucrt64\lib -lassimp -lglfw3 -l:libz.a -lcomdlg32 -lole32 -lshell32 -lwinhttp ^
    -L%NET%\target\release -lrunbun_net -lbcrypt -ladvapi32 -lntdll -luserenv -lws2_32 -ldbghelp ^
    -static-libstdc++ -static-libgcc ^
    -o %OUT%\RunBunSandbox.exe || goto fail

xcopy /e /i /y /q assets\* %OUT%\ >nul
for %%D in (glfw3 libassimp-6 libbz2-1 libgcc_s_seh-1 libminizip-1 libstdc++-6 libwinpthread-1 zlib1) do (
    if not exist "%OUT%\%%D.dll" if exist "%MSYS%\ucrt64\bin\%%D.dll" copy /y "%MSYS%\ucrt64\bin\%%D.dll" "%OUT%" >nul
)

echo Built %OUT%\RunBunSandbox.exe (build %RUNBUN_BUILD%)
exit /b 0

:fail
echo Build failed.
exit /b 1
