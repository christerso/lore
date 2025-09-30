# AEON Origin Adjuster - Complete User Guide

## Overview

The AEON Origin Adjuster is a professional Blender addon designed specifically for game developers to quickly and accurately set object origins for optimal game engine integration.

## Installation

1. Open Blender
2. Go to `Edit > Preferences > Add-ons`
3. Click `Install...` and select `origin_adjuster_addon.py`
4. Enable the addon by checking the box next to "AEON Origin Adjuster"
5. The tool will appear in the 3D View sidebar under "AEON Tools"

## Features

### 1. Set Standing Origin
**Icon:** Anchor Bottom
**Purpose:** Sets origin to bottom center of object
**Use Cases:** Characters, props, furniture, weapons, vehicles - anything that "stands" on surfaces
**Result:** Origin positioned at bottom center of bounding box

### 2. Set Floor Origin
**Icon:** Anchor Top
**Purpose:** Sets origin to top center of object
**Use Cases:** Floor tiles, platforms, ceilings, roof panels - anything that forms surfaces
**Result:** Origin positioned at top center of bounding box

### 3. Batch Adjust by Name
**Icon:** Auto
**Purpose:** Automatically detects object type by name and applies appropriate origin setting
**Use Cases:** Processing multiple imported assets at once
**Result:** Each object gets the correct origin type based on naming conventions

## How Batch Adjust by Name Works

The Batch Adjust feature analyzes each selected object's name and automatically determines the appropriate origin type:

### Floor Object Detection
Objects with names containing these keywords get **Floor Origin** (top center):
- `floor`
- `tile`
- `platform`
- `ground`
- `ceiling`
- `roof`

### Standing Object Detection
All other objects get **Standing Origin** (bottom center)

### Example Usage
If you select these objects and click "Batch Adjust by Name":

| Object Name | Detected Type | Origin Applied |
|-------------|---------------|----------------|
| `player_character` | Standing Object | Bottom center |
| `floor_tile_01` | Floor Object | Top center |
| `wall_section` | Standing Object | Bottom center |
| `ceiling_panel` | Floor Object | Top center |
| `chair_prop` | Standing Object | Bottom center |

## Usage Instructions

### Single Object
1. Select a mesh object
2. Choose the appropriate origin type (Standing or Floor)
3. Click the corresponding button

### Multiple Objects
1. Select multiple mesh objects
2. Choose the appropriate origin type (Standing or Floor)
3. Click the corresponding button
4. Each object will be processed individually with its own origin point

### Batch Processing by Name
1. Select multiple mesh objects with descriptive names
2. Click "Batch Adjust by Name"
3. The addon will automatically apply the correct origin type to each object

## Game Development Benefits

### Standing Objects (Bottom Center Origin)
- Characters stand correctly on ground
- Props sit properly on surfaces
- Easy placement in level editors
- Consistent height alignment
- Natural physics behavior

### Floor Objects (Top Center Origin)
- Tiles snap together perfectly
- Platforms align at surface level
- Ceilings attach correctly to structures
- Easy surface-to-surface placement
- Consistent building workflows

## Technical Details

### Multi-Object Support
- Each object is processed individually
- Original selections are preserved
- Objects can be at different positions
- No cross-object interference

### Position Independence
- Works regardless of object location in world space
- Handles rotated and scaled objects
- Maintains object transformations
- Compatible with parent-child relationships

### Error Handling
- Graceful handling of invalid selections
- Individual object error reporting
- Safe operation - won't break your scene
- Detailed feedback messages

## Naming Convention Tips

For best results with Batch Adjust by Name:

### Floor Objects
- `floor_tile_01`
- `platform_section`
- `ground_plane`
- `ceiling_panel`
- `roof_tile`

### Standing Objects
- `character_player`
- `prop_chair`
- `weapon_sword`
- `vehicle_car`
- `furniture_table`

## Troubleshooting

### Common Issues

**"No mesh objects selected"**
- Ensure you have mesh objects selected (not curves, empties, etc.)
- Check that objects are visible and selectable

**"Failed to process [object name]"**
- Object may be in an invalid state
- Try applying all modifiers first
- Check for mesh errors with Blender's mesh analysis tools

**Objects not getting correct origins in batch mode**
- Verify object names follow the keyword conventions
- Check for typos in keywords
- Ensure objects are properly named before batching

### Tips for Best Results

1. **Apply modifiers** before setting origins
2. **Use consistent naming** for batch processing
3. **Work in object mode** for most reliable results
4. **Save your work** before batch processing large selections
5. **Test on single objects** before using batch mode

## Game Engine Compatibility

### Unity
- Standing objects: Perfect for character controllers and props
- Floor objects: Ideal for tilemap systems and platforms

### Unreal Engine
- Standing objects: Works with character movement components
- Floor objects: Compatible with landscape and BSP systems

### Godot
- Standing objects: Native support for 2D and 3D character nodes
- Floor objects: Perfect for tilemap and grid-based systems

## Version History

### Version 1.0.0
- Initial release
- Standing and Floor origin adjustment
- Multi-object support
- Batch processing by name
- Position-independent operation
- Fixed bmesh API compatibility issues

## Support

For issues, feature requests, or questions:
- Check the troubleshooting section first
- Verify you're using the latest version
- Ensure Blender compatibility (3.0+)

---

**AEON Engine Development Team**
*Professional tools for game developers*