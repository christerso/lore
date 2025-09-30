@echo off
REM Script to check C++ source files formatting in the Lore project
REM Usage: check_format.bat

echo Checking C++ source files formatting in Lore Engine...
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this script from the project root.
    exit /b 1
)

REM Find the build directory
if exist "cmake-build-debug" (
    set BUILD_DIR=cmake-build-debug
) else if exist "cmake-build-debug-v2022" (
    set BUILD_DIR=cmake-build-debug-v2022
) else if exist "cmake-build-release" (
    set BUILD_DIR=cmake-build-release
) else (
    echo Error: No build directory found. Please run CMake configuration first.
    echo Example: configure_2022.bat
    exit /b 1
)

echo Using build directory: %BUILD_DIR%
echo.

REM Check if the build directory is configured
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Error: Build directory not configured. Please run CMake configuration first.
    echo Example: configure_2022.bat
    exit /b 1
)

REM Run the format check target
echo Checking code formatting...
cmake --build %BUILD_DIR% --target format_check

if %ERRORLEVEL% EQU 0 (
    echo.
    echo All files are properly formatted!
) else (
    echo.
    echo Formatting issues found. Run 'format_code.bat' to fix.
    echo.
    echo Specific files can be checked manually:
    echo   clang-format --dry-run --Werror path\to\file.cpp
    exit /b 1
)