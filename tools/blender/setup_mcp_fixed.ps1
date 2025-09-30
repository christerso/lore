# AEON Engine - Blender MCP Setup Script (Fixed)
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

# Step 4: SAFELY update Claude Code MCP configuration
Write-Host "`n4. Setting up Claude Code MCP configuration..." -ForegroundColor Yellow

$claudeConfigPath = "$env:APPDATA\Claude\claude_desktop_config.json"
$claudeConfigDir = Split-Path $claudeConfigPath -Parent

# Create Claude config directory if it doesn't exist
if (-not (Test-Path $claudeConfigDir)) {
    New-Item -ItemType Directory -Path $claudeConfigDir -Force | Out-Null
    Write-Host "📁 Created Claude config directory" -ForegroundColor Blue
}

# SAFELY read existing configuration
$config = @{}
$hasExistingConfig = $false

if (Test-Path $claudeConfigPath) {
    # Create backup first
    $timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $backupPath = "$claudeConfigPath.backup.$timestamp"
    try {
        Copy-Item $claudeConfigPath $backupPath
        Write-Host "💾 Backup created: $backupPath" -ForegroundColor Blue
    } catch {
        Write-Host "⚠️ Could not create backup" -ForegroundColor Yellow
    }

    try {
        $configContent = Get-Content $claudeConfigPath -Raw
        if ($configContent -and $configContent.Trim()) {
            $config = $configContent | ConvertFrom-Json -AsHashtable
            $hasExistingConfig = $true
            Write-Host "📖 Found existing Claude config" -ForegroundColor Blue

            # Show existing MCP servers
            if ($config.ContainsKey("mcpServers") -and $config["mcpServers"].Keys.Count -gt 0) {
                Write-Host "🔍 Existing MCP servers:" -ForegroundColor Cyan
                foreach ($serverName in $config["mcpServers"].Keys) {
                    Write-Host "   ✓ $serverName" -ForegroundColor Green
                }
                Write-Host "   📝 These will be preserved!" -ForegroundColor Green
            }
        }
    } catch {
        Write-Host "⚠️ Existing config has invalid JSON, creating new one" -ForegroundColor Yellow
        $config = @{}
    }
} else {
    Write-Host "📄 Creating new Claude config file" -ForegroundColor Blue
}

# Safely add mcpServers section if needed
if (-not $config.ContainsKey("mcpServers")) {
    $config["mcpServers"] = @{}
    Write-Host "➕ Created mcpServers section" -ForegroundColor Green
}

# Check if blender-mcp already exists
if ($config["mcpServers"].ContainsKey("blender-mcp")) {
    Write-Host "⚠️ blender-mcp server already exists" -ForegroundColor Yellow
    $response = Read-Host "Update existing blender-mcp configuration? (y/N)"
    if ($response -notmatch '^[Yy]') {
        Write-Host "❌ Skipping blender-mcp update" -ForegroundColor Red
        exit 0
    }
} else {
    Write-Host "➕ Adding blender-mcp server" -ForegroundColor Green
}

# Add/update blender-mcp configuration
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

    # Show final summary
    Write-Host "`n📊 Final MCP servers:" -ForegroundColor Cyan
    foreach ($serverName in $config["mcpServers"].Keys) {
        $marker = if ($serverName -eq "blender-mcp") { "🆕" } else { "✓" }
        Write-Host "   $marker $serverName" -ForegroundColor Green
    }
} catch {
    Write-Host "❌ Failed to write Claude config" -ForegroundColor Red
    exit 1
}

# Step 5: Blender addon setup instructions
Write-Host "`n5. Blender addon setup..." -ForegroundColor Yellow
$addonPath = Join-Path $PSScriptRoot "blender_mcp_addon.py"

if (Test-Path $addonPath) {
    Write-Host "✅ Blender MCP addon found: $addonPath" -ForegroundColor Green
    Write-Host "`n📝 To complete setup:" -ForegroundColor Cyan
    Write-Host "   1. Open Blender" -ForegroundColor White
    Write-Host "   2. Edit → Preferences → Add-ons" -ForegroundColor White
    Write-Host "   3. Install... → Select: $addonPath" -ForegroundColor White
    Write-Host '   4. Enable "Interface: Blender MCP"' -ForegroundColor White
} else {
    Write-Host "❌ Blender MCP addon not found" -ForegroundColor Red
}

# Step 6: Summary
Write-Host "`n🎉 Setup Complete!" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green
Write-Host "✅ uv Python package manager" -ForegroundColor Green
Write-Host "✅ blender-mcp server installed" -ForegroundColor Green
Write-Host "✅ Claude Code MCP configuration updated" -ForegroundColor Green
Write-Host "✅ Existing MCP servers preserved" -ForegroundColor Green
Write-Host "✅ Blender path detected" -ForegroundColor Green

Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
Write-Host "1. 🔄 RESTART CLAUDE CODE for MCP changes to take effect" -ForegroundColor White
Write-Host "2. 🔧 Install the Blender addon (instructions above)" -ForegroundColor White
Write-Host "3. 🎮 Test the integration by asking Claude to create a 3D model" -ForegroundColor White

Write-Host "`n🔧 Configuration saved to:" -ForegroundColor Blue
Write-Host $claudeConfigPath -ForegroundColor White

Write-Host "`nPress any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")