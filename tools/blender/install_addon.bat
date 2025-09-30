@echo off
echo ========================================
echo AEON Engine - Blender Tools Installer
echo ========================================
echo.

REM Find Blender installation
set "BLENDER_PATH="
set "ADDON_PATH="

REM Common Blender installation paths
for %%p in (
    "C:\Program Files\Blender Foundation\Blender *\*\scripts\addons"
    "C:\Program Files (x86)\Blender Foundation\Blender *\*\scripts\addons"
    "%APPDATA%\Blender Foundation\Blender\*\scripts\addons"
    "%USERPROFILE%\AppData\Roaming\Blender Foundation\Blender\*\scripts\addons"
) do (
    if exist "%%~p" (
        set "ADDON_PATH=%%~p"
        goto :found
    )
)

:found
if "%ADDON_PATH%"=="" (
    echo ❌ Blender installation not found!
    echo Please install the addon manually:
    echo 1. Open Blender
    echo 2. Go to Edit ^> Preferences ^> Add-ons
    echo 3. Click "Install..." and select: game_object_setup.py
    echo 4. Enable the addon by checking the box
    pause
    exit /b 1
)

echo ✅ Found Blender addons directory: %ADDON_PATH%
echo.

REM Copy the addon
echo Installing AEON Game Object Setup addon...
copy "game_object_setup.py" "%ADDON_PATH%\" >nul 2>&1

if %ERRORLEVEL%==0 (
    echo ✅ Addon installed successfully!
    echo.
    echo 📝 Next steps:
    echo 1. Open Blender
    echo 2. Go to Edit ^> Preferences ^> Add-ons
    echo 3. Search for "Game Object Setup"
    echo 4. Enable the addon by checking the box
    echo 5. Look for the "Game Dev" panel in the 3D Viewport
) else (
    echo ❌ Failed to copy addon. You may need to run as administrator.
    echo.
    echo Manual installation:
    echo 1. Copy game_object_setup.py to: %ADDON_PATH%
    echo 2. Or install via Blender's Add-ons preferences
)

echo.
echo ========================================
pause