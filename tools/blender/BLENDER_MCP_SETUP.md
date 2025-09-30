# NEONTECH Blender MCP Setup Guide

**Goal**: Connect Claude Code directly to Blender for automated 3D model creation

---

## 🚀 **Quick Setup (5 minutes)**

### **1. Install Blender Addon**
1. **Copy the addon file**: `blender_mcp_addon.py` is ready in the root directory
2. **Open Blender**
3. **Edit** → **Preferences** → **Add-ons**
4. **Click "Install..."** button
5. **Select** `blender_mcp_addon.py`
6. **Enable** the addon: Check "Interface: Blender MCP"
7. **Save Preferences**

### **2. Configure Claude Desktop**
✅ **ALREADY DONE** - Configuration added to:
`C:\Users\chris\AppData\Roaming\Claude\claude_desktop_config.json`

### **3. Connect in Blender**
1. **Open Blender** (fresh instance)
2. **3D Viewport** → Press `N` to show sidebar
3. **Find "BlenderMCP" tab** in the sidebar
4. **Click "Connect to Claude"**
5. **Verify connection** - should show "Connected" status

### **4. Test Connection**
Once connected, Claude can directly:
- Create 3D models
- Apply materials 
- Generate NEONTECH building blocks
- Export models automatically

---

## 🔧 **Verification Steps**

### **Check #1: Addon Installed**
- In Blender Preferences → Add-ons
- Search for "MCP"
- Should show "Interface: Blender MCP" with checkmark

### **Check #2: MCP Tab Visible**
- In 3D Viewport sidebar (press `N`)
- Should see "BlenderMCP" tab
- Contains "Connect to Claude" button

### **Check #3: Connection Working**
- Click "Connect to Claude"
- Status should change from "Disconnected" to "Connected"
- No error messages in Blender console

---

## 🎯 **What This Enables**

Once set up, Claude can directly:

### **Create Models**
```python
# Claude can run commands like:
create_1x1_wall_block()
create_floor_tile("ONYX_BLACK")
create_server_rack_assembly()
```

### **Apply Materials**
```python
# Automatic PBR material setup:
apply_terrazzo_white_material(wall_object)
setup_onyx_black_floor(floor_object)
```

### **Export for NEONTECH**
```python
# Batch export with optimization:
export_neontech_models(output_path)
generate_model_manifest()
```

---

## ⚠️ **Troubleshooting**

### **Problem**: Can't find BlenderMCP tab
**Solution**: 
- Restart Blender after installing addon
- Check addon is enabled in Preferences
- Try pressing `N` in 3D viewport

### **Problem**: Connection fails
**Solution**:
- Restart Claude Desktop application
- Restart Blender
- Check Windows firewall isn't blocking connection

### **Problem**: Addon won't install
**Solution**:
- Make sure `blender_mcp_addon.py` file isn't corrupted
- Try installing as ZIP file
- Check Blender version is 3.0+

---

## 📁 **File Locations**

- **Addon**: `blender_mcp_addon.py` (root directory)
- **Claude Config**: `%APPDATA%\Claude\claude_desktop_config.json`
- **MCP Server**: Installed via `uvx blender-mcp`

---

## 🎮 **Next Steps After Setup**

1. **Test basic cube creation** via Claude
2. **Generate 1×1 meter reference blocks**
3. **Create NEONTECH building set**
4. **Export models to `assets/models/`**

This integration allows Claude to directly create professional 3D models for NEONTECH without manual Blender work!