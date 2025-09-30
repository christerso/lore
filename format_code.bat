@echo off
REM Script to format all C++ source files in the Lore project
REM Usage: format_code.bat

echo Formatting all C++ source files in Lore Engine...
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

REM Run the format target
echo Running clang-format on all source files...
cmake --build %BUILD_DIR% --target format

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Code formatting completed successfully!
    echo.
    echo To check formatting without making changes, run:
    echo   cmake --build %BUILD_DIR% --target format_check
    echo.
    echo To format specific files, run:
    echo   cmake --build %BUILD_DIR% --target format_file -- path\to\file.cpp path\to\file.hpp
) else (
    echo.
    echo Error: Code formatting failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)