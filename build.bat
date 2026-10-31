@echo off
rem Builds RunBun Sandbox into src\RunBunSandbox.exe
rem Uses the MSYS2 mingw64 g++ with Assimp/GLM headers from ucrt64.

set MSYS=C:\msys64
set PATH=%MSYS%\mingw64\bin;%PATH%

cd /d "%~dp0src"
if not exist ..\build mkdir ..\build

rem C files get compiled on their own with gcc
gcc -O2 -Iinclude -c src\glad.c -o ..\build\glad.o || goto fail
gcc -O2 -c tinyfiledialogs\tinyfiledialogs.c -o ..\build\tinyfiledialogs.o || goto fail

g++ -std=gnu++20 -O2 ^
    -I. -Iimgui -Iinclude -idirafter %MSYS%\ucrt64\include ^
    *.cpp ^
    imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\imgui_demo.cpp ^
    imgui\backends\imgui_impl_glfw.cpp imgui\backends\imgui_impl_opengl3.cpp ^
    ..\build\glad.o ..\build\tinyfiledialogs.o ^
    -L%MSYS%\ucrt64\lib -lassimp -lglfw3 -lcomdlg32 -lole32 -lshell32 ^
    -static-libstdc++ -static-libgcc ^
    -o RunBunSandbox.exe || goto fail
rem static C++ runtime: the libstdc++ DLL next to the exe is the ucrt64 one and doesn't match this compiler

echo Built src\RunBunSandbox.exe
exit /b 0

:fail
echo Build failed.
exit /b 1
