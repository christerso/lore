# AEON Interior Toolkit - Blender 4.5+ Installation Guide

## 🚀 Blender 4.5+ Compatibility

This toolkit has been optimized for Blender 4.5+ with proper context handling and modern API usage.

### ✅ What's Compatible

- **All Operators**: Fully compatible with Blender 4.5+
- **Dialog System**: Uses modern context override methods
- **UI Panels**: Standard Blender UI components
- **Property Management**: Compatible with latest property system
- **bmesh Operations**: Updated for current bmesh API

### 🔧 Key Compatibility Features

#### Context Override (Blender 4.5+)
```python
# Modern Blender 4.5+ method
context_override = bpy.context.copy()
with bpy.context.temp_override(**context_override):
    bpy.ops.aeon.tile_manager_dialog('INVOKE_DEFAULT')
```

#### Operator Registration
- All operators use modern registration patterns
- Compatible with Blender's new addon validation system
- Proper error handling for different Blender versions

#### UI System
- Uses standard Blender UI components
- Compatible with both old and new panel systems
- Works with Blender's theme system

## 📦 Installation Instructions

### Method 1: Standard Installation (Recommended)

1. **Open Blender 4.5+**
2. Go to `Edit > Preferences > Add-ons`
3. Click `Install...`
4. Navigate to and select `aeon_interior_toolkit_enhanced.py`
5. Enable the addon by checking the box
6. Look for "AEON Tools" in the 3D View sidebar

### Method 2: Copy to Addons Directory

1. **Locate your Blender addons directory:**
   - Windows: `%APPDATA%\Blender Foundation\Blender\4.5\scripts\addons\`
   - macOS: `/Users/username/Library/Application Support/Blender/4.5/scripts/addons/`
   - Linux: `/home/username/.config/blender/4.5/scripts/addons/`

2. **Copy the addon file:**
   ```bash
   # Copy the enhanced toolkit to addons directory
   cp aeon_interior_toolkit_enhanced.py /path/to/blender/addons/
   ```

3. **Enable in Blender:**
   - Restart Blender
   - Go to `Edit > Preferences > Add-ons`
   - Search for "AEON" and enable the addon

## 🧪 Testing the Installation

### Quick Test Script

Run the test suite to verify everything works:

1. **Open Blender 4.5+**
2. **Switch to Scripting workspace**
3. **Create a new text block**
4. **Paste the test script content** from `test_enhanced_toolkit.py`
5. **Run the script** (click the "Run Script" button)

### Expected Results

The test suite should show:
- ✅ Blender 4.5+ compatibility tests passing
- ✅ All operators registered successfully
- ✅ Dialog functionality working
- ✅ All core features operational

### Manual Test

If the test suite fails, try these manual steps:

1. **Check if addon is enabled:**
   - Look in `Edit > Preferences > Add-ons`
   - Search for "AEON Interior Toolkit"
   - Ensure it's enabled

2. **Check the UI:**
   - Switch to 3D View
   - Press `N` to open sidebar
   - Look for "AEON Tools" tab

3. **Test basic functionality:**
   - Click "Tile Manager Dialog"
   - Try creating a simple room template
   - Test the quick view buttons

## 🔍 Troubleshooting Blender 4.5+ Issues

### Issue: Addon won't enable
**Solution:**
- Check Blender version (4.5+ required)
- Verify file is not corrupted
- Check Python syntax in addon file

### Issue: Dialog windows don't open
**Solution:**
- Ensure you're in 3D View
- Try restarting Blender
- Check console for error messages

### Issue: Operators not found
**Solution:**
- Re-enable the addon
- Check if addon is properly installed
- Verify no conflicting addons

### Issue: Context override errors
**Solution:**
- Update to latest Blender 4.5+ version
- Ensure proper area context
- Check if script is run from correct workspace

## 🎯 First Steps After Installation

### 1. Create Your First Tile Template
1. Create a simple cube (floor tile)
2. Select it and click "Create Room Template"
3. Name it "floor_tile_01"
4. Choose category "Floor"

### 2. Test the Room Wizard
1. Click "Room Creation Wizard"
2. Follow the step-by-step interface
3. Create a simple 5x5 room
4. Verify it appears correctly

### 3. Try the Enhanced Tile Painter
1. Open "Enhanced Tile Painter"
2. Select your created template
3. Click "Place Mode"
4. Try painting tiles on the grid

## 📚 API Reference for Developers

### Context Override Usage
```python
# For Blender 4.5+
context_override = bpy.context.copy()
with bpy.context.temp_override(**context_override):
    bpy.ops.aeon.your_operator('INVOKE_DEFAULT')

# For older Blender versions
bpy.ops.aeon.your_operator('INVOKE_DEFAULT')
```

### Operator Calling
```python
# All operators are available under bpy.ops.aeon
bpy.ops.aeon.tile_manager_dialog()
bpy.ops.aeon.room_wizard_dialog()
bpy.ops.aeon.enhanced_tile_painter()
```

## 🔄 Updates and Maintenance

### Checking for Updates
- Monitor the GitHub repository for updates
- Check for Blender version compatibility updates
- Run test suite after Blender updates

### Updating the Addon
1. Download the latest version
2. Replace the old file in addons directory
3. Restart Blender
4. Run test suite to verify functionality

---

## 🎉 Getting Help

### If you encounter issues:
1. Check this guide first
2. Run the test suite
3. Check Blender console for error messages
4. Refer to the main README.md file

### Success Indicators:
- ✅ All tests pass
- ✅ AEON Tools tab appears in 3D View sidebar
- ✅ Dialogs open correctly
- ✅ All operators function as expected

**Enjoy creating beautiful tile-based interiors with the AEON Interior Toolkit!**