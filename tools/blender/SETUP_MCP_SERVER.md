# AEON Engine - Blender MCP Server Setup

Complete guide to integrate Blender MCP server with Claude Code for AI-powered 3D model creation.

## 🚀 **Quick Setup Steps**

### **1. Install Blender MCP Server**
```bash
# In PowerShell/Command Prompt:
uvx install blender-mcp
```

### **2. Add MCP Server to Claude Code**
You need to add this configuration to Claude Code. The exact method depends on your Claude Code version:

**Option A: Using Claude Code Command**
```bash
claude add mcp blender-mcp
```

**Option B: Manual Configuration File**
Add to your Claude Code MCP configuration file:

```json
{
  "mcpServers": {
    "blender-mcp": {
      "command": "uvx",
      "args": ["blender-mcp"],
      "env": {
        "BLENDER_PATH": "C:\\Program Files\\Blender Foundation\\Blender\\blender.exe"
      }
    }
  }
}
```

### **3. Restart Claude Code**
**IMPORTANT**: Claude Code must be restarted for MCP changes to take effect.

### **4. Install Blender Addon**
```bash
# In AEON project directory:
cd G:/repos/aeon/tools/blender/
./install_addon.bat
```

## 📋 **Detailed Configuration**

### **Blender Path Detection**
The MCP server needs to know where Blender is installed. Common paths:

```bash
# Standard Windows installation:
C:\Program Files\Blender Foundation\Blender\blender.exe

# Alternative locations:
C:\Program Files (x86)\Blender Foundation\Blender\blender.exe
%USERPROFILE%\AppData\Local\Programs\Blender Foundation\Blender\blender.exe

# Steam version:
C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe
```

### **Environment Variables**
Set these for optimal MCP integration:

```bash
# Windows Environment Variables:
BLENDER_PATH=C:\Program Files\Blender Foundation\Blender\blender.exe
BLENDER_MCP_PORT=9876
BLENDER_MCP_HOST=localhost
```

### **Firewall Configuration**
Ensure Windows Firewall allows connections on MCP port:
- **Port**: 9876 (default)
- **Protocol**: TCP
- **Direction**: Inbound/Outbound
- **Program**: Claude Code and Blender

## 🔧 **Manual MCP Configuration**

If automatic setup doesn't work, manually configure Claude Code:

### **Configuration File Locations**
Claude Code configuration is typically stored in:
```
Windows: %APPDATA%\Claude\claude_code_config.json
macOS: ~/Library/Application Support/Claude/claude_code_config.json
Linux: ~/.config/claude/claude_code_config.json
```

### **Complete MCP Configuration**
```json
{
  "mcpServers": {
    "blender-mcp": {
      "command": "uvx",
      "args": ["blender-mcp"],
      "env": {
        "BLENDER_PATH": "C:\\Program Files\\Blender Foundation\\Blender\\blender.exe",
        "BLENDER_MCP_PORT": "9876",
        "BLENDER_MCP_HOST": "localhost"
      },
      "cwd": "G:\\repos\\aeon\\tools\\blender"
    }
  }
}
```

## ✅ **Verification Steps**

### **1. Check MCP Server Installation**
```bash
uvx list
# Should show "blender-mcp" in the list
```

### **2. Test MCP Server Connection**
```bash
uvx blender-mcp --test
# Should connect to Blender and report success
```

### **3. Verify Claude Code Integration**
After restart, Claude Code should show:
- MCP connection status in status bar
- Available Blender tools in command palette
- No MCP connection errors in logs

### **4. Test Blender Addon**
1. Open Blender
2. Check for "BlenderMCP" tab in 3D Viewport sidebar (press N)
3. Click "Connect to Claude" button
4. Status should show "Connected"

## 🎯 **Usage After Setup**

Once configured, you can ask Claude Code to:

```
"Create a 1x1 meter stone wall tile for AEON with weathered texture"
"Generate a medieval wooden door with iron hinges"
"Create a server rack assembly with LED indicators"
"Make a laboratory workstation with equipment"
```

Claude will:
1. Connect to Blender via MCP
2. Generate the 3D model
3. Apply appropriate materials
4. Position for AEON game use
5. Export in game-ready format

## 🔍 **Troubleshooting**

### **Problem**: uvx command not found
**Solution**: Install uv first:
```bash
pip install uv
```

### **Problem**: MCP connection fails
**Solutions**:
- Restart Claude Code after configuration
- Check Blender path is correct
- Verify firewall settings
- Try manual port specification

### **Problem**: Blender addon not visible
**Solutions**:
- Restart Blender after addon installation
- Check addon is enabled in Preferences
- Verify Python path in Blender

### **Problem**: Models not exporting correctly
**Solutions**:
- Check AEON export path exists
- Verify file permissions
- Ensure Blender has write access to output directory

## 📁 **File Structure After Setup**

```
G:/repos/aeon/tools/blender/
├── blender_mcp_addon.py          # Blender addon (install in Blender)
├── game_object_setup.py          # Game object preparation
├── SETUP_MCP_SERVER.md           # This guide
├── AEON_BLENDER_SUITE.md         # Complete documentation
└── NEONTECH tools/               # Advanced generators
```

## 🚀 **Next Steps**

1. **Install MCP server**: `uvx install blender-mcp`
2. **Configure Claude Code**: Add MCP server configuration
3. **Restart Claude Code**: Required for MCP integration
4. **Install Blender addon**: Run `install_addon.bat`
5. **Test integration**: Ask Claude to create a simple model

This setup creates a complete AI-powered 3D modeling pipeline for AEON game development! 🎮✨