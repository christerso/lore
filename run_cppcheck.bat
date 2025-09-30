@echo off
REM Script to run cppcheck static analysis on the Lore project
REM Usage: run_cppcheck.bat [html|suggest]

set MODE=%1

echo Running cppcheck static analysis on Lore Engine...
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

REM Choose the appropriate target based on mode
if "%MODE%"=="html" (
    echo Generating HTML report...
    set TARGET=cppcheck_html
    echo Report will be available in: %BUILD_DIR%\cppcheck-report\index.html
) else if "%MODE%"=="suggest" (
    echo Running cppcheck with improvement suggestions...
    set TARGET=cppcheck_suggest
) else (
    echo Running standard cppcheck analysis...
    set TARGET=cppcheck
    echo Results will be saved to: %BUILD_DIR%\cppcheck-results.xml
)

echo.

REM Run the cppcheck target
cmake --build %BUILD_DIR% --target %TARGET%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo cppcheck analysis completed successfully!
    echo.
    echo Available commands:
    echo   run_cppcheck.bat          - Standard analysis (XML output)
    echo   run_cppcheck.bat html     - HTML report generation
    echo   run_cppcheck.bat suggest  - Analysis with suggestions
    echo.
    echo Suppressions file: .cppcheck_suppressions
    echo Edit this file to suppress false positives.
) else (
    echo.
    echo cppcheck analysis completed with issues found.
    echo Error code: %ERRORLEVEL%
    echo.
    echo Review the output above for details.

    if "%MODE%"=="" (
        echo XML results saved to: %BUILD_DIR%\cppcheck-results.xml
        echo View with: cppcheck-gui %BUILD_DIR%\cppcheck-results.xml
    )

    exit /b %ERRORLEVEL%
)