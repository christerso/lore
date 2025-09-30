# AEON Engine - Origin Adjuster Addon Installer
# Professional installation script for Blender addon

Write-Host "🎮 AEON Engine - Origin Adjuster Addon Installer" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan

# Function to find Blender installation
function Find-BlenderPath {
    $blenderPaths = @(
        "C:\Program Files\Blender Foundation\Blender 4.5\blender.exe",
        "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe",
        "C:\Program Files\Blender Foundation\Blender 4.3\blender.exe",
        "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe",
        "C:\Program Files\Blender Foundation\Blender 4.1\blender.exe",
        "C:\Program Files\Blender Foundation\Blender 4.0\blender.exe",
        "C:\Program Files\Blender Foundation\Blender\blender.exe",
        "C:\Program Files (x86)\Blender Foundation\Blender\blender.exe"
    )

    foreach ($path in $blenderPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    return $null
}

# Function to get Blender version and addon directory
function Get-BlenderAddonPath {
    param($blenderPath)

    $blenderDir = Split-Path $blenderPath -Parent
    $versionPattern = $blenderDir | Select-String -Pattern "(\d+\.\d+)"

    if ($versionPattern) {
        $version = $versionPattern.Matches[0].Groups[1].Value
        $userPath = "$env:APPDATA\Blender Foundation\Blender\$version\scripts\addons"
        return $userPath
    }

    # Fallback - try to find any Blender addon directory
    $blenderAppData = "$env:APPDATA\Blender Foundation\Blender"
    if (Test-Path $blenderAppData) {
        $versionDirs = Get-ChildItem $blenderAppData -Directory | Sort-Object Name -Descending
        if ($versionDirs.Count -gt 0) {
            return "$($versionDirs[0].FullName)\scripts\addons"
        }
    }

    return $null
}

Write-Host "`n🔍 Searching for Blender installation..." -ForegroundColor Yellow

$blenderPath = Find-BlenderPath

if (-not $blenderPath) {
    Write-Host "❌ Blender not found in standard locations" -ForegroundColor Red
    $customPath = Read-Host "Please enter the full path to blender.exe"

    if (-not (Test-Path $customPath)) {
        Write-Host "❌ Invalid path. Installation cancelled." -ForegroundColor Red
        exit 1
    }
    $blenderPath = $customPath
}

Write-Host "✅ Found Blender at: $blenderPath" -ForegroundColor Green

# Get addon installation directory
$addonPath = Get-BlenderAddonPath $blenderPath

if (-not $addonPath) {
    Write-Host "❌ Could not determine Blender addon directory" -ForegroundColor Red
    exit 1
}

Write-Host "📁 Addon directory: $addonPath" -ForegroundColor Blue

# Create addon directory if it doesn't exist
if (-not (Test-Path $addonPath)) {
    try {
        New-Item -ItemType Directory -Path $addonPath -Force | Out-Null
        Write-Host "📁 Created addon directory" -ForegroundColor Blue
    } catch {
        Write-Host "❌ Failed to create addon directory: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

# Check if addon file exists
$sourceAddon = Join-Path $PSScriptRoot "origin_adjuster_addon.py"
if (-not (Test-Path $sourceAddon)) {
    Write-Host "❌ Origin adjuster addon file not found: $sourceAddon" -ForegroundColor Red
    exit 1
}

# Install the addon
$destinationAddon = Join-Path $addonPath "aeon_origin_adjuster.py"

try {
    # Check if addon already exists
    if (Test-Path $destinationAddon) {
        Write-Host "⚠️ AEON Origin Adjuster addon already exists" -ForegroundColor Yellow
        $overwrite = Read-Host "Do you want to overwrite it? (y/N)"
        if ($overwrite -notmatch '^[Yy]') {
            Write-Host "❌ Installation cancelled by user" -ForegroundColor Red
            exit 0
        }
    }

    # Copy addon file
    Copy-Item $sourceAddon $destinationAddon -Force
    Write-Host "✅ AEON Origin Adjuster addon installed successfully!" -ForegroundColor Green

    # Show installation summary
    Write-Host "`n📋 Installation Summary:" -ForegroundColor Cyan
    Write-Host "├─ Addon: AEON Origin Adjuster v1.0.0" -ForegroundColor White
    Write-Host "├─ Location: $destinationAddon" -ForegroundColor White
    Write-Host "└─ Blender: $blenderPath" -ForegroundColor White

    Write-Host "`n🚀 Next Steps:" -ForegroundColor Cyan
    Write-Host "1. 🎨 Open Blender" -ForegroundColor White
    Write-Host "2. ⚙️ Go to Edit → Preferences → Add-ons" -ForegroundColor White
    Write-Host "3. 🔍 Search for 'AEON Origin Adjuster'" -ForegroundColor White
    Write-Host "4. ✅ Enable the addon" -ForegroundColor White
    Write-Host "5. 🧰 Find it in 3D Viewport → Sidebar → AEON Tools tab" -ForegroundColor White

    Write-Host "`n💡 Features:" -ForegroundColor Cyan
    Write-Host "• 🏠 Standing Objects: Origin at bottom center (props, furniture)" -ForegroundColor Green
    Write-Host "• 🏢 Floor Objects: Origin at top center (tiles, platforms)" -ForegroundColor Green
    Write-Host "• 🔄 Batch Processing: Auto-detect by object name" -ForegroundColor Green
    Write-Host "• 🎯 Smart Detection: Suggests object type based on naming" -ForegroundColor Green

} catch {
    Write-Host "❌ Failed to install addon: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "`n🎉 Installation Complete!" -ForegroundColor Green
Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")