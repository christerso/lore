@echo off
echo Checking for compile_commands.json...
if exist "%~1\compile_commands.json" (
    echo Found compile_commands.json, copying to project root...
    copy /Y "%~1\compile_commands.json" "%~2\compile_commands.json" >nul
    echo compile_commands.json copied successfully.
) else (
    echo compile_commands.json not found - skipping copy
)