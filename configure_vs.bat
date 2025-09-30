@echo off
echo Setting up Visual Studio 2026 environment...
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

echo Adding Program Files Git to PATH...
set "PATH=C:\Program Files\Git\bin;%PATH%"

echo Running CMake with Visual Studio generator...
"C:\Users\chris\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" ^
-DCMAKE_BUILD_TYPE=Debug ^
-G "Visual Studio 17 2022" ^
-A x64 ^
-T v146 ^
-S "G:\repos\lore" ^
-B "G:\repos\lore\cmake-build-debug-vs"

echo CMake configuration completed.