# AEON Origin Adjuster - Blender Addon

Professional origin adjustment tools for game development in Blender.

## 🎯 Purpose

When creating 3D assets for games, proper origin point placement is crucial for:
- **Standing Objects** (props, characters, furniture): Origin at bottom center for proper floor placement
- **Floor Objects** (tiles, platforms, ceilings): Origin at top center for seamless tiling

## ✨ Features

### 🏠 Standing Object Mode
- Sets origin to **bottom center** of the object
- Perfect for props that need to sit on floors
- Ideal for characters, furniture, decorative objects

### 🏢 Floor Object Mode
- Sets origin to **top center** of the object
- Perfect for floor tiles and platforms
- Ensures seamless tiling and stacking

### 🔄 Batch Processing
- Auto-detects object type based on naming conventions
- Processes multiple objects simultaneously
- Smart keyword recognition (floor, tile, platform, ground, ceiling, roof)

### 🎯 Smart UI
- Visual object type detection
- Clear instructions and feedback
- Professional game development workflow

## 🚀 Installation

### Automatic Installation (Recommended)
1. Open PowerShell in the `G:\repos\aeon\tools\blender\` directory
2. Run: `.\install_origin_adjuster.ps1`
3. Follow the installation prompts
4. Open Blender and enable the addon in Preferences

### Manual Installation
1. Copy `origin_adjuster_addon.py` to your Blender addons folder:
   - Windows: `%APPDATA%\Blender Foundation\Blender\[VERSION]\scripts\addons\`
   - Rename to: `aeon_origin_adjuster.py`
2. Open Blender → Edit → Preferences → Add-ons
3. Search for "AEON Origin Adjuster"
4. Enable the addon

## 🧰 Usage

### Location
Find the addon in: **3D Viewport → Sidebar (N key) → AEON Tools tab**

### Basic Workflow
1. **Select** one or more mesh objects
2. **Choose** the appropriate origin type:
   - **Standing Origin**: For objects that stand on floors
   - **Floor Origin**: For floor tiles and platforms
3. **Click** the corresponding button

### Batch Processing
1. **Select** multiple mesh objects with descriptive names
2. **Click** "Batch Adjust by Name"
3. The addon automatically detects object types based on keywords

## 🎮 Game Development Best Practices

### Standing Objects
- Characters and NPCs
- Furniture (chairs, tables, beds)
- Props (barrels, crates, decorations)
- Vehicles and machinery
- Trees and vegetation

### Floor Objects
- Floor tiles and planks
- Platform sections
- Ceiling tiles
- Ground textures
- Modular building pieces

## 🔧 Technical Details

### Algorithm
- **Bounding Box Analysis**: Calculates precise object bounds
- **Non-Destructive**: Moves mesh data to maintain object positioning
- **World Space Accurate**: Works correctly with scaled and rotated objects
- **Undo Support**: Full Blender undo/redo integration

### Naming Conventions
The batch processor recognizes these keywords for floor objects:
- `floor`, `tile`, `platform`, `ground`, `ceiling`, `roof`

Examples:
- `FloorTile_01` → Detected as floor object
- `WoodPlatform` → Detected as floor object
- `Chair_Modern` → Detected as standing object

## 🐛 Troubleshooting

### "No mesh objects selected"
- Ensure you have mesh objects selected
- The addon only works with mesh objects, not cameras, lights, etc.

### Origin doesn't move as expected
- Check if the object has applied transforms (Ctrl+A)
- Ensure the object isn't parented with transform inheritance issues

### Batch processing not working correctly
- Verify object names contain recognizable keywords
- Use manual mode for objects with ambiguous names

## 📋 Requirements

- **Blender**: 3.0.0 or higher
- **Object Type**: Mesh objects only
- **Mode**: Works in Object and Edit modes

## 🎨 Interface

The addon provides a clean, professional interface with:
- Object type detection display
- Clear action buttons with icons
- Usage instructions
- Selection feedback
- Progress reporting

## 🔄 Workflow Integration

This addon integrates seamlessly with:
- **AEON Engine**: Optimized for AEON game development
- **Game Asset Pipelines**: Standard origin conventions
- **Blender MCP**: Works alongside other AEON tools
- **Export Tools**: Ensures proper origins for game engines

## 📝 Version History

### v1.0.0
- Initial release
- Standing object origin adjustment
- Floor object origin adjustment
- Batch processing with name detection
- Professional UI with smart detection
- Complete error handling and user feedback

## 🤝 Support

For issues, feature requests, or contributions:
- Check the AEON Engine documentation
- Review the addon code for customization
- Test with simple objects first

## 📄 License

MIT License - Part of the AEON Engine development toolkit.

---

**Happy Game Development!** 🎮