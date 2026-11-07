@echo off
setlocal

set MSYS=C:\msys64
set PATH=%MSYS%\mingw64\bin;%PATH%
set TP=third_party
set OBJ=build
set OUT=bin

cd /d "%~dp0"
if not exist %OBJ% mkdir %OBJ%
if not exist %OUT% mkdir %OUT%

gcc -O2 -I%TP%\glad\include -c %TP%\glad\src\glad.c -o %OBJ%\glad.o || goto fail
gcc -O2 -c %TP%\tinyfiledialogs\tinyfiledialogs.c -o %OBJ%\tinyfiledialogs.o || goto fail

g++ -std=gnu++20 -O2 ^
    -Isrc -I%TP% -I%TP%\imgui -I%TP%\glad\include -idirafter %MSYS%\ucrt64\include ^
    src\*.cpp src\core\*.cpp src\render\*.cpp src\ui\*.cpp ^
    %TP%\imgui\imgui.cpp %TP%\imgui\imgui_draw.cpp %TP%\imgui\imgui_tables.cpp %TP%\imgui\imgui_widgets.cpp %TP%\imgui\imgui_demo.cpp ^
    %TP%\imgui\backends\imgui_impl_glfw.cpp %TP%\imgui\backends\imgui_impl_opengl3.cpp ^
    %OBJ%\glad.o %OBJ%\tinyfiledialogs.o ^
    -L%MSYS%\ucrt64\lib -lassimp -lglfw3 -lcomdlg32 -lole32 -lshell32 ^
    -static-libstdc++ -static-libgcc ^
    -o %OUT%\RunBunSandbox.exe || goto fail

xcopy /e /i /y /q assets\* %OUT%\ >nul
for %%D in (glfw3 libassimp-6 libbz2-1 libgcc_s_seh-1 libminizip-1 libstdc++-6 libwinpthread-1 zlib1) do (
    if not exist "%OUT%\%%D.dll" copy /y "%MSYS%\ucrt64\bin\%%D.dll" "%OUT%" >nul
)

echo Built %OUT%\RunBunSandbox.exe
exit /b 0

:fail
echo Build failed.
exit /b 1
