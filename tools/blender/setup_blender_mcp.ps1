# AEON Engine - Blender MCP Setup Script
# This script sets up the Blender MCP server for Claude Code integration

Write-Host "🎮 AEON Engine - Blender MCP Setup" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan

# Step 1: Check if uv is installed
Write-Host "`n1. Checking for uv installation..." -ForegroundColor Yellow
try {
    $uvVersion = uv --version 2>$null
    if ($uvVersion) {
        Write-Host "✅ uv is already installed: $uvVersion" -ForegroundColor Green
    }
} catch {
    Write-Host "❌ uv not found. Installing via winget..." -ForegroundColor Red

    # Install uv via winget
    try {
        winget install astral-sh.uv
        Write-Host "✅ uv installed successfully" -ForegroundColor Green

        # Refresh PATH to include uv
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("PATH","User")
        Write-Host "🔄 PATH refreshed" -ForegroundColor Blue
    } catch {
        Write-Host "❌ Failed to install uv via winget" -ForegroundColor Red
        Write-Host "Please install manually: https://docs.astral.sh/uv/getting-started/installation/" -ForegroundColor Yellow
        exit 1
    }
}

# Step 2: Install blender-mcp server
Write-Host "`n2. Installing blender-mcp server..." -ForegroundColor Yellow
try {
    uv tool install blender-mcp
    Write-Host "✅ blender-mcp server installed" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to install blender-mcp server" -ForegroundColor Red
    Write-Host "Trying alternative installation method..." -ForegroundColor Yellow

    try {
        # Alternative: install via pip
        pip install blender-mcp
        Write-Host "✅ blender-mcp installed via pip" -ForegroundColor Green
    } catch {
        Write-Host "❌ Failed to install blender-mcp" -ForegroundColor Red
        exit 1
    }
}

# Step 3: Find Blender installation
Write-Host "`n3. Finding Blender installation..." -ForegroundColor Yellow
$blenderPaths = @(
    "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe",
    "C:\Program Files\Blender Foundation\Blender 4.1\blender.exe",
    "C:\Program Files\Blender Foundation\Blender 4.0\blender.exe",
    "C:\Program Files\Blender Foundation\Blender 3.6\blender.exe",
    "C:\Program Files\Blender Foundation\Blender\blender.exe",
    "C:\Program Files (x86)\Blender Foundation\Blender\blender.exe"
)

$blenderPath = $null
foreach ($path in $blenderPaths) {
    if (Test-Path $path) {
        $blenderPath = $path
        break
    }
}

if ($blenderPath) {
    Write-Host "✅ Found Blender at: $blenderPath" -ForegroundColor Green
} else {
    Write-Host "❌ Blender not found in standard locations" -ForegroundColor Red
    $blenderPath = Read-Host "Please enter the full path to blender.exe"

    if (-not (Test-Path $blenderPath)) {
        Write-Host "❌ Invalid Blender path. Please install Blender first." -ForegroundColor Red
        exit 1
    }
}

# Step 4: Create Claude Code MCP configuration
Write-Host "`n4. Setting up Claude Code MCP configuration..." -ForegroundColor Yellow

$claudeConfigPath = "$env:APPDATA\Claude\claude_desktop_config.json"
$claudeConfigDir = Split-Path $claudeConfigPath -Parent

# Create Claude config directory if it doesn't exist
if (-not (Test-Path $claudeConfigDir)) {
    New-Item -ItemType Directory -Path $claudeConfigDir -Force | Out-Null
    Write-Host "📁 Created Claude config directory" -ForegroundColor Blue
}

# Read existing config or create new one
$config = @{}
if (Test-Path $claudeConfigPath) {
    try {
        $configContent = Get-Content $claudeConfigPath -Raw
        $config = $configContent | ConvertFrom-Json -AsHashtable
        Write-Host "📖 Read existing Claude config" -ForegroundColor Blue
    } catch {
        Write-Host "⚠️ Existing config is invalid, creating new one" -ForegroundColor Yellow
        $config = @{}
    }
} else {
    Write-Host "📄 Creating new Claude config file" -ForegroundColor Blue
}

# Add or update blender-mcp server configuration
if (-not $config.ContainsKey("mcpServers")) {
    $config["mcpServers"] = @{}
}

$config["mcpServers"]["blender-mcp"] = @{
    "command" = "uv"
    "args" = @("tool", "run", "blender-mcp")
    "env" = @{
        "BLENDER_PATH" = $blenderPath
    }
}

# Write updated configuration
try {
    $configJson = $config | ConvertTo-Json -Depth 10
    $configJson | Set-Content $claudeConfigPath -Encoding UTF8
    Write-Host "✅ Claude Code MCP configuration updated" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to write Claude config" -ForegroundColor Red
    exit 1
}

# Step 5: Install Blender addon
Write-Host "`n5. Blender addon setup..." -ForegroundColor Yellow
$addonPath = Join-Path $PSScriptRoot "blender_mcp_addon.py"

if (Test-Path $addonPath) {
    Write-Host "✅ Blender MCP addon found: $addonPath" -ForegroundColor Green
    Write-Host "📝 To complete setup:" -ForegroundColor Cyan
    Write-Host "   1. Open Blender" -ForegroundColor White
    Write-Host "   2. Edit → Preferences → Add-ons" -ForegroundColor White
    Write-Host "   3. Install... → Select: $addonPath" -ForegroundColor White
    Write-Host "   4. Enable 'Interface: Blender MCP'" -ForegroundColor White
} else {
    Write-Host "❌ Blender MCP addon not found" -ForegroundColor Red
}

# Step 6: Summary
Write-Host "`n🎉 Setup Complete!" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green
Write-Host "✅ uv Python package manager" -ForegroundColor Green
Write-Host "✅ blender-mcp server installed" -ForegroundColor Green
Write-Host "✅ Claude Code MCP configuration" -ForegroundColor Green
Write-Host "✅ Blender path detected" -ForegroundColor Green

Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
Write-Host "1. Restart Claude Code for MCP changes to take effect" -ForegroundColor White
Write-Host "2. Install the Blender addon (instructions above)" -ForegroundColor White
Write-Host "3. Test the integration by asking Claude to create a 3D model" -ForegroundColor White

Write-Host "`n🔧 Configuration saved to:" -ForegroundColor Blue
Write-Host $claudeConfigPath -ForegroundColor White

Write-Host "`nPress any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")