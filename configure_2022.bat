@echo off
echo Setting up Visual Studio 2022 environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo Adding Program Files Git to PATH...
set "PATH=C:\Program Files\Git\bin;%PATH%"

echo Running CMake with Visual Studio 2022 generator...
"C:\Users\chris\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" ^
-DCMAKE_BUILD_TYPE=Debug ^
-G "Visual Studio 17 2022" ^
-A x64 ^
-T v143 ^
-S "G:\repos\lore" ^
-B "G:\repos\lore\cmake-build-debug-v2022"

echo CMake configuration completed.
echo You can now open the project in Visual Studio 2022 or build with: cd cmake-build-debug-v2022 && cmake --build .