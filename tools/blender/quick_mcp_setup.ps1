# AEON Engine - Quick Blender MCP Setup
Write-Host "Installing Blender MCP Server..." -ForegroundColor Cyan

# Install blender-mcp server
Write-Host "Installing blender-mcp via uv..." -ForegroundColor Yellow
try {
    uv tool install blender-mcp
    Write-Host "✅ blender-mcp server installed" -ForegroundColor Green
} catch {
    Write-Host "Trying pip instead..." -ForegroundColor Yellow
    pip install blender-mcp
    Write-Host "✅ blender-mcp installed via pip" -ForegroundColor Green
}

# Find Blender
$blenderPath = "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe"
if (-not (Test-Path $blenderPath)) {
    $blenderPath = "C:\Program Files\Blender Foundation\Blender 4.1\blender.exe"
}
if (-not (Test-Path $blenderPath)) {
    $blenderPath = "C:\Program Files\Blender Foundation\Blender\blender.exe"
}

Write-Host "Using Blender at: $blenderPath" -ForegroundColor Green

# Setup MCP config
$claudeConfigPath = "$env:APPDATA\Claude\claude_desktop_config.json"
$claudeConfigDir = Split-Path $claudeConfigPath -Parent

if (-not (Test-Path $claudeConfigDir)) {
    New-Item -ItemType Directory -Path $claudeConfigDir -Force | Out-Null
}

$config = @{}
if (Test-Path $claudeConfigPath) {
    $configContent = Get-Content $claudeConfigPath -Raw
    if ($configContent -and $configContent.Trim()) {
        $config = $configContent | ConvertFrom-Json -AsHashtable
    }
}

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

$configJson = $config | ConvertTo-Json -Depth 10
$configJson | Set-Content $claudeConfigPath -Encoding UTF8

Write-Host "✅ MCP configuration updated" -ForegroundColor Green
Write-Host "📋 Next: RESTART CLAUDE CODE" -ForegroundColor Cyan