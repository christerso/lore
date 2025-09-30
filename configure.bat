@echo off
echo Setting up Visual Studio 2026 environment...
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

echo Adding Program Files Git to PATH...
set "PATH=C:\Program Files\Git\bin;%PATH%"

echo Setting up additional CMake variables for VS 2026...
set "CC=cl.exe"
set "CXX=cl.exe"

echo Running CMake with Ninja generator...
"C:\Users\chris\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" ^
-DCMAKE_BUILD_TYPE=Debug ^
-DCMAKE_MAKE_PROGRAM="C:\Users\chris\AppData\Local\Programs\CLion\bin\ninja\win\x64\ninja.exe" ^
-G "Ninja" ^
-DCMAKE_C_COMPILER="cl.exe" ^
-DCMAKE_CXX_COMPILER="cl.exe" ^
-DCMAKE_LINKER="link.exe" ^
-DCMAKE_RC_COMPILER="rc.exe" ^
-S "G:\repos\lore" ^
-B "G:\repos\lore\cmake-build-debug"

echo CMake configuration completed.