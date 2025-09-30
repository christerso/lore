# PowerShell Script to Update Claude Configuration with Obsidian MCP

# Configuration to add
$obsidianConfig = @{
    mcpServers = @{
        obsidian = @{
            command = "python"
            args = @("-m", "mcp_obsidian")
            env = @{
                OBSIDIAN_API_KEY = "1c8e4c45baa6d96fffcac88c1fc92a30554d2336af4009603281f12ff4ee04b3"
                OBSIDIAN_HOST = "localhost"
                OBSIDIAN_PORT = "27123"
            }
        }
        "obsidian-advanced" = @{
            command = "python"
            args = @("G:/repos/lore/scripts/advanced_obsidian_mcp.py")
            env = @{
                OBSIDIAN_API_KEY = "1c8e4c45baa6d96fffcac88c1fc92a30554d2336af4009603281f12ff4ee04b3"
                OBSIDIAN_HOST = "localhost"
                OBSIDIAN_PORT = "27123"
                OBSIDIAN_DEBUG = "false"
                OBSIDIAN_CACHE_SIZE = "1000"
                OBSIDIAN_TIMEOUT = "30"
            }
        }
    }
}

$toolsToAdd = @(
    "Edit",
    "Read",
    "Write",
    "Glob",
    "Grep",
    "Bash",
    "BashOutput",
    "WebSearch",
    "WebFetch",
    "Task",
    "TodoWrite",
    "SlashCommand",
    "mcp__obsidian__*",
    "mcp__obsidian-advanced__*"
)

# Possible locations for Claude configuration
$possiblePaths = @(
    "$env:USERPROFILE\.claude\settings.json",
    "$env:APPDATA\Claude\claude_desktop_config.json",
    "$env:USERPROFILE\AppData\Roaming\Claude\claude_desktop_config.json"
)

Write-Host "Searching for existing Claude configuration files..." -ForegroundColor Yellow

$foundConfig = $null
$configPath = $null

foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        Write-Host "Found configuration at: $path" -ForegroundColor Green
        $foundConfig = Get-Content $path -Raw | ConvertFrom-Json
        $configPath = $path
        break
    }
}

if (-not $foundConfig) {
    Write-Host "No existing Claude configuration found." -ForegroundColor Red
    Write-Host "Creating new configuration file..." -ForegroundColor Yellow

    # Create new configuration
    $newConfig = @{
        mcpServers = $obsidianConfig.mcpServers
        allowedTools = $toolsToAdd
    }

    # Try to create in the most likely location
    $targetPath = "$env:USERPROFILE\.claude\settings.json"
    $targetDir = Split-Path $targetPath -Parent

    if (-not (Test-Path $targetDir)) {
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    }

    $newConfig | ConvertTo-Json -Depth 10 | Set-Content $targetPath
    Write-Host "Created new configuration at: $targetPath" -ForegroundColor Green
    Write-Host "Please restart Claude to load the new configuration." -ForegroundColor Yellow
    exit
}

Write-Host "Current configuration:" -ForegroundColor Cyan
$foundConfig | ConvertTo-Json -Depth 10 | Write-Host

# Merge configurations
Write-Host "Merging Obsidian MCP configuration..." -ForegroundColor Yellow

# Add/Update MCP servers
if (-not $foundConfig.PSObject.Properties['mcpServers']) {
    $foundConfig | Add-Member -MemberType NoteProperty -Name 'mcpServers' -Value @{}
}

foreach ($serverName in $obsidianConfig.mcpServers.Keys) {
    if (-not $foundConfig.mcpServers.PSObject.Properties[$serverName]) {
        $foundConfig.mcpServers | Add-Member -MemberType NoteProperty -Name $serverName -Value $obsidianConfig.mcpServers[$serverName]
        Write-Host "Added server: $serverName" -ForegroundColor Green
    } else {
        $foundConfig.mcpServers[$serverName] = $obsidianConfig.mcpServers[$serverName]
        Write-Host "Updated server: $serverName" -ForegroundColor Yellow
    }
}

# Add/Update allowed tools
if (-not $foundConfig.PSObject.Properties['allowedTools']) {
    $foundConfig | Add-Member -MemberType NoteProperty -Name 'allowedTools' -Value @()
}

$existingTools = $foundConfig.allowedTools
foreach ($tool in $toolsToAdd) {
    if ($tool -like "mcp__*" -and $existingTools -like "mcp__*") {
        # Remove existing MCP tools to avoid duplicates
        $foundConfig.allowedTools = $foundConfig.allowedTools | Where-Object { $_ -notlike "mcp__*" }
    }
}

# Add new tools
foreach ($tool in $toolsToAdd) {
    if ($tool -notin $foundConfig.allowedTools) {
        $foundConfig.allowedTools += $tool
        Write-Host "Added tool: $tool" -ForegroundColor Green
    }
}

# Backup original config
$backupPath = "$configPath.backup.$(Get-Date -Format 'yyyyMMdd_HHmmss')"
Copy-Item $configPath $backupPath
Write-Host "Created backup at: $backupPath" -ForegroundColor Cyan

# Save updated configuration
$foundConfig | ConvertTo-Json -Depth 10 | Set-Content $configPath

Write-Host "Configuration updated successfully!" -ForegroundColor Green
Write-Host "Updated file: $configPath" -ForegroundColor Cyan
Write-Host "Please restart Claude to load the new configuration." -ForegroundColor Yellow

# Show what was added
Write-Host "`nSummary of changes:" -ForegroundColor Magenta
Write-Host "• Added/Updated Obsidian MCP servers" -ForegroundColor White
Write-Host "• Added Obsidian-specific tools to allowedTools" -ForegroundColor White
Write-Host "• Your existing configuration has been preserved" -ForegroundColor White

Write-Host "`nTo test the configuration:" -ForegroundColor Cyan
Write-Host "1. Install Obsidian Local REST API plugin" -ForegroundColor White
Write-Host "2. Run: python G:/repos/lore/scripts/test_obsidian_mcp.py" -ForegroundColor White
Write-Host "3. Restart Claude and check for Obsidian tools" -ForegroundColor White