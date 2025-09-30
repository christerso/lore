# AEON Interior Toolkit - Tile-Based Level Creation

## Overview

The AEON Interior Toolkit is a specialized Blender addon designed specifically for tile-based interior level creation. It provides everything needed to rapidly create game levels with tile placement, portal systems, trigger events, and seamless engine integration.

## Installation

### Requirements
- **Blender 4.5 or later** (required for full compatibility)
- Python 3.8+ (included with Blender)

### Quick Installation

1. **Validate Compatibility** (recommended):
   - Open Blender 4.5+
   - Run `validate_blender_45.py` script
   - Ensure compatibility score is 75% or higher

2. **Install Addon**:
   - Go to `Edit > Preferences > Add-ons`
   - Click `Install...` and select `aeon_interior_toolkit_enhanced.py`
   - Enable the addon by checking the box
   - The toolkit will appear in the 3D View sidebar under "AEON Tools"

3. **Test Installation**:
   - Run `test_enhanced_toolkit.py` to verify all features work

### Detailed Installation Guide
See `BLENDER_45_INSTALLATION.md` for comprehensive Blender 4.5+ installation instructions.

## Core Features

### 🏗️ **Room Template System**

**Create Room Templates**
- Save entire room layouts as reusable templates
- Include furniture, props, and metadata
- Template preview system
- Quick placement with one click

**Usage:**
1. Build a room layout with tiles and objects
2. Select all objects in the room
3. Click "Create Room Template"
4. Give it a descriptive name
5. Use "Place Room Template" to instantiate it

### 🎨 **Tile Painter System**

**Visual Tile Painting**
- Click-to-paint tile placement
- Visual preview grid system
- Multiple paint modes (Place, Remove, Fill, Paint)
- Tile category filtering
- Grid snapping with customizable size

**Paint Modes:**
- **Place**: Single tile placement
- **Remove**: Remove individual tiles
- **Paint**: Continuous painting while holding mouse
- **Fill**: Flood fill connected areas

**Tile Categories:**
- Floor tiles
- Wall tiles
- Props and objects
- Special function tiles

### 🚪 **Portal & Transition System**

**Level Transition Management**
- Mark objects as portals/entrances
- Define target maps and connection points
- Bidirectional portal support
- Various portal types (doors, teleporters, level transitions)

**Portal Types:**
- **Door**: Standard door portal
- **Teleporter**: Instant teleportation
- **Level Transition**: Map loading portal
- **Secret**: Hidden portal

**Usage:**
1. Select an object (door frame, etc.)
2. Click "Mark as Portal"
3. Set target map name
4. Configure portal properties

### 👁️ **Quick View System**

**Rapid View Switching**
- One-click view presets for level design
- Optimized views for different tasks
- Orthographic and perspective modes

**View Presets:**
- **Top**: Top-down orthographic view
- **Front**: Front orthographic view
- **Side**: Side orthographic view
- **ISO**: Isometric view
- **Tile**: Special top view for tile placement
- **Persp**: Standard perspective view

**Keyboard Shortcuts:**
- Can be mapped to number keys for rapid switching
- Perfect for level design workflow

### ⚡ **Trigger & Event System**

**Game Logic Integration**
- Attach triggers to any object
- Multiple trigger types (On Enter, On Use, On Touch, etc.)
- JSON parameter system for game events
- Cooldown and activation control

**Trigger Types:**
- **On Enter**: Trigger when object enters area
- **On Exit**: Trigger when object leaves area
- **On Use**: Trigger when player interacts
- **On Touch**: Trigger on contact
- **On Proximity**: Trigger when near object
- **On Timer**: Time-delayed triggers

**Usage:**
1. Select an object (door, switch, etc.)
2. Click "Attach Trigger"
3. Configure trigger type and event
4. Add custom parameters in JSON format

### 📤 **Level Data Export**

**Engine-Ready Export**
- Export complete level data to JSON
- Include tiles, portals, and triggers
- Ready for C++ game engine integration
- Metadata and version control

**Export Options:**
- Include/exclude specific data types
- Multiple export formats (JSON, Binary, Custom)
- File path configuration

## Workflow Guide

### Setting Up Your Project

1. **Create Tile Templates**
   - Model your basic tiles (floor, walls, etc.)
   - Set appropriate origins using Origin Adjuster
   - Ensure consistent scale (1 unit = 1 tile recommended)

2. **Configure Tile Categories**
   - Organize tiles by category (Floor, Wall, Prop, Special)
   - Add descriptive names for easy identification
   - Set up preview materials

3. **Set Up Grid System**
   - Use Quick View → Tile for optimal placement view
   - Configure grid size in Tile Painter settings
   - Enable grid snapping for precise placement

### Creating a Room Layout

1. **Manual Tile Placement**
   - Select Tile Painter tool
   - Choose paint mode (Place/Paint/Fill)
   - Filter by category if needed
   - Click to place tiles

2. **Using Room Templates**
   - Create template from existing layout
   - Use template list to select
   - Click "Place Room Template"
   - Position as needed

3. **Adding Portals**
   - Place door frames or entrance objects
   - Mark as portal with target map
   - Configure portal behavior
   - Test connections

### Adding Game Logic

1. **Trigger Placement**
   - Select interactive objects
   - Attach trigger with appropriate type
   - Configure event and parameters
   - Set cooldown if needed

2. **Event Configuration**
   - Use JSON format for parameters
   - Example: `{"speed": 1.0, "sound": "door_open.wav"}`
   - Reference target objects by name
   - Include game-specific data

3. **Testing Integration**
   - Export level data to JSON
   - Load in your game engine
   - Verify trigger and portal behavior
   - Iterate as needed

### Optimizing Workflow

1. **View Management**
   - Use Tile view for placement
   - Switch to ISO for visual checking
   - Use Top view for overall layout
   - Quick view switching saves time

2. **Template Organization**
   - Create templates for common room types
   - Name descriptively (kitchen_10x10, corridor_5x20)
   - Include furniture in templates
   - Update templates as needed

3. **Performance Considerations**
   - Use tile painter for efficient placement
   - Group similar objects
   - Use templates for repeated layouts
   - Export only necessary data

## Technical Implementation

### Data Structures

**Tile Data:**
```json
{
  "type": "tile",
  "name": "floor_tile_01",
  "position": [x, y, z],
  "rotation": [rx, ry, rz],
  "scale": [sx, sy, sz],
  "mesh": "floor_tile_mesh"
}
```

**Portal Data:**
```json
{
  "name": "main_entrance",
  "target_map": "level02.map",
  "target_portal": "level02_entrance",
  "type": "door",
  "is_bidirectional": true,
  "load_trigger": "on_touch"
}
```

**Trigger Data:**
```json
{
  "name": "door_switch",
  "type": "on_use",
  "event": "open_door",
  "target_object": "door_01",
  "parameters": {
    "speed": 1.0,
    "sound": "door_open.wav"
  },
  "is_active": true,
  "cooldown": 0.0
}
```

### C++ Integration

**Loading Level Data:**
```cpp
struct LevelData {
    std::string name;
    std::vector<TileData> tiles;
    std::vector<PortalData> portals;
    std::vector<TriggerData> triggers;
};

// Load from JSON
LevelData loadLevel(const std::string& filename);
```

**Trigger System:**
```cpp
class Trigger {
public:
    void checkCollision(GameObject* entity);
    void fireEvent(const std::string& event);
    void setCooldown(float time);
};
```

### Portal System
```cpp
class Portal {
public:
    void transitionTo(Level* targetLevel);
    void setBidirectional(bool bidirectional);
    bool canActivate(GameObject* entity);
};
```

## Best Practices

### Tile Design
- Keep tile sizes consistent (power of 2 recommended)
- Design tiles to connect seamlessly
- Use collision-friendly geometry
- Optimize vertex count for performance

### Level Design
- Plan room flow before building
- Use templates for consistent layouts
- Mark portals early in the process
- Test trigger placement thoroughly

### Performance Optimization
- Limit visible tiles at once
- Use LOD for complex tiles
- Group similar objects for batching
- Export only necessary data

### Workflow Tips
- Save templates frequently
- Use multiple view modes
- Test portal connections
- Verify trigger parameters

## Troubleshooting

### Common Issues

**"No tile templates available"**
- Create tile templates first
- Ensure objects are properly categorized
- Check object selection

**"Portal not working"**
- Verify target map name spelling
- Check portal object collision
- Ensure trigger type is correct

**"Trigger not firing"**
- Check trigger activation conditions
- Verify event name spelling
- Ensure object is in collision layer

**Export issues**
- Check file path permissions
- Verify JSON format
- Ensure all references exist

### Tips for Success

1. **Start Simple**: Begin with basic tiles and expand
2. **Test Often**: Verify functionality frequently
3. **Use Templates**: Save time with reusable layouts
4. **Plan Ahead**: Design room flow before building
5. **Document**: Keep track of trigger and portal names

## Version History

### Version 1.0.0
- Initial release
- Tile painter system
- Room template management
- Portal and transition system
- Trigger and event system
- Quick view switching
- Level data export

## Future Enhancements

Planned features for future versions:
- Procedural room generation
- Advanced trigger visualizers
- Collision shape generation
- Lighting baking tools
- Multi-theme support
- Collaborative editing features

## Support

For issues, bug reports, or feature requests:
- Check the troubleshooting section
- Verify Blender version compatibility (3.0+)
- Ensure proper installation

---

**AEON Engine Development Team**
*Built by game developers, for game developers*

## Special Thanks

To the indie game development community for inspiration and feedback:
- Tile-based game developers
- Level designers and artists
- C++ game engine programmers
- Blender tools developers

This toolkit was created to solve real problems in real game development workflows.