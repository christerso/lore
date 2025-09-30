# AEON Engine - Blender MCP Server Setup Script
# Automates installation and configuration of Blender MCP integration

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AEON Engine - Blender MCP Server Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Install uv if not available
Write-Host "Step 1: Checking uv installation..." -ForegroundColor Yellow
try {
    $uvVersion = uv --version 2>$null
    if ($uvVersion) {
        Write-Host "✅ uv is installed: $uvVersion" -ForegroundColor Green
    }
} catch {
    Write-Host "❌ uv not found. Installing uv..." -ForegroundColor Red
    try {
        pip install uv
        Write-Host "✅ uv installed successfully" -ForegroundColor Green
    } catch {
        Write-Host "❌ Failed to install uv. Please install manually: pip install uv" -ForegroundColor Red
        exit 1
    }
}

# Step 2: Install Blender MCP server
Write-Host ""
Write-Host "Step 2: Installing Blender MCP server..." -ForegroundColor Yellow
try {
    uvx install blender-mcp
    Write-Host "✅ Blender MCP server installed successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to install Blender MCP server" -ForegroundColor Red
    Write-Host "Please run manually: uvx install blender-mcp" -ForegroundColor Yellow
}

# Step 3: Find Blender installation
Write-Host ""
Write-Host "Step 3: Locating Blender installation..." -ForegroundColor Yellow
$blenderPaths = @(
    "C:\Program Files\Blender Foundation\Blender\*\blender.exe",
    "C:\Program Files (x86)\Blender Foundation\Blender\*\blender.exe",
    "$env:USERPROFILE\AppData\Local\Programs\Blender Foundation\Blender\*\blender.exe",
    "C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe"
)

$blenderPath = $null
foreach ($path in $blenderPaths) {
    $found = Get-ChildItem $path -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $blenderPath = $found.FullName
        break
    }
}

if ($blenderPath) {
    Write-Host "✅ Blender found at: $blenderPath" -ForegroundColor Green
} else {
    Write-Host "❌ Blender not found. Please install Blender first." -ForegroundColor Red
    $blenderPath = "C:\Program Files\Blender Foundation\Blender\blender.exe"
    Write-Host "Using default path: $blenderPath" -ForegroundColor Yellow
}

# Step 4: Create MCP configuration
Write-Host ""
Write-Host "Step 4: Creating MCP configuration..." -ForegroundColor Yellow

$mcpConfig = @{
    mcpServers = @{
        "blender-mcp" = @{
            command = "uvx"
            args = @("blender-mcp")
            env = @{
                BLENDER_PATH = $blenderPath
                BLENDER_MCP_PORT = "9876"
                BLENDER_MCP_HOST = "localhost"
            }
            cwd = "G:\repos\aeon\tools\blender"
        }
    }
} | ConvertTo-Json -Depth 4

$configPath = Join-Path $PSScriptRoot "claude_mcp_config.json"
$mcpConfig | Out-File -FilePath $configPath -Encoding UTF8
Write-Host "✅ MCP configuration created: $configPath" -ForegroundColor Green

# Step 5: Display configuration instructions
Write-Host ""
Write-Host "Step 5: Claude Code Configuration Instructions" -ForegroundColor Yellow
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To complete the setup, you need to add the MCP server to Claude Code:" -ForegroundColor White
Write-Host ""
Write-Host "OPTION A: Using Claude Code Command" -ForegroundColor Cyan
Write-Host "1. In Claude Code, run: claude add mcp blender-mcp" -ForegroundColor White
Write-Host ""
Write-Host "OPTION B: Manual Configuration" -ForegroundColor Cyan
Write-Host "1. Find your Claude Code configuration file:" -ForegroundColor White
Write-Host "   Windows: %APPDATA%\Claude\claude_code_config.json" -ForegroundColor Gray
Write-Host "2. Add the configuration from: $configPath" -ForegroundColor White
Write-Host ""
Write-Host "OPTION C: Use the generated config" -ForegroundColor Cyan
Write-Host "1. Copy the contents of: $configPath" -ForegroundColor White
Write-Host "2. Paste into your Claude Code MCP configuration" -ForegroundColor White

# Step 6: Blender addon installation
Write-Host ""
Write-Host "Step 6: Blender Addon Installation" -ForegroundColor Yellow
Write-Host "===================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Run the Blender addon installer:" -ForegroundColor White
Write-Host "   .\install_addon.bat" -ForegroundColor Gray
Write-Host ""
Write-Host "Or install manually in Blender:" -ForegroundColor White
Write-Host "1. Edit → Preferences → Add-ons" -ForegroundColor Gray
Write-Host "2. Install... → Select blender_mcp_addon.py" -ForegroundColor Gray
Write-Host "3. Enable 'Interface: Blender MCP'" -ForegroundColor Gray

# Step 7: Final instructions
Write-Host ""
Write-Host "Step 7: Final Steps" -ForegroundColor Yellow
Write-Host "==================" -ForegroundColor Cyan
Write-Host ""
Write-Host "🔄 RESTART CLAUDE CODE after adding MCP configuration" -ForegroundColor Red
Write-Host "🔄 RESTART BLENDER after installing addon" -ForegroundColor Red
Write-Host ""
Write-Host "After restart, test the integration:" -ForegroundColor White
Write-Host "1. Open Blender → 3D Viewport → Press N → BlenderMCP tab" -ForegroundColor Gray
Write-Host "2. Click 'Connect to Claude'" -ForegroundColor Gray
Write-Host "3. Ask Claude Code to create 3D models!" -ForegroundColor Gray

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Setup script completed!" -ForegroundColor Green
Write-Host "Next: Configure Claude Code and restart" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan

# Display the configuration for easy copying
Write-Host ""
Write-Host "📋 MCP Configuration to add to Claude Code:" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host $mcpConfig -ForegroundColor Gray