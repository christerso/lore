# AEON Game Developer Toolkit - Complete Guide

## Overview

The AEON Game Developer Toolkit is a comprehensive Blender addon designed specifically for C++/Vulkan/DirectX/shader-based game development. It provides specialized tools for mesh optimization, vertex data processing, tile-based interior building, and engine-specific asset preparation.

## Installation

1. Open Blender
2. Go to `Edit > Preferences > Add-ons`
3. Click `Install...` and select the desired addon file:
   - `aeon_game_dev_toolkit.py` - Core game development tools
   - `aeon_tile_interior_tools.py` - Tile-based interior tools
4. Enable the addon by checking the box
5. Tools will appear in the 3D View sidebar under "AEON Tools"

## Core Game Development Tools

### Mesh Optimization

#### Vertex Cache Optimizer
**Purpose:** Optimize vertex order for GPU cache efficiency
**Use:** Reduces GPU vertex shader workload by 20-60%
**Ideal For:** All game assets, especially complex models

#### LOD Generator
**Purpose:** Generate multiple Level of Detail models
**Settings:**
- LOD Levels (1-5)
- Reduction Ratio per level (0.1-0.9)
**Output:** Separate mesh objects with `_LOD1`, `_LOD2` suffixes

### Vertex Data Tools

#### Tangent Space Calculator
**Purpose:** Generate perfect tangent, bitangent for normal mapping
**Critical For:** Shader-based normal mapping in Vulkan/DirectX
**Requirements:** Object must have UV maps

#### Lighting to Vertex Baker
**Purpose:** Bake simple ambient occlusion into vertex colors
**Use Cases:**
- Low-poly assets with baked lighting
- Mobile game optimization
- Vertex-based lighting effects

### UV & Texture Tools

#### UV Atlas Generator
**Purpose:** Pack multiple UV charts into efficient atlas
**Settings:**
- Texture Size (256-8192)
- Padding in pixels (1-32)
**Use For:** Texture atlasing, draw call optimization

#### Texel Density Checker
**Purpose:** Analyze and normalize texel density across assets
**Target Density:** Configurable pixels per meter
**Output:** Console report of density per object

### Shader Pipeline Tools

#### Shader Constant Prep
**Purpose:** Extract material properties for shader constants
**Output Types:**
- Diffuse colors
- Roughness/Metallic values
- Specular properties
**Export:** Console output (ready for C++ integration)

### Export Tools

#### Engine-Specific Optimizer
**Purpose:** Apply engine-specific optimizations
**Supported Engines:**
- Unity (naming conventions, collision setup)
- Unreal Engine (specific requirements)
- Custom engines (generic optimizations)

**Features:**
- Apply all modifiers
- Clean up unused vertex groups
- Remove unused data layers
- Engine-specific naming

## Tile-Based Interior Tools

### Grid System

#### Tile Grid Setup
**Purpose:** Create precise grid system for tile placement
**Settings:**
- Grid Size (0.1-10.0 units)
- Grid Count (5-100 lines)
- Floor reference plane
- Custom floor height

**Features:**
- Visual grid lines using empties
- Optional floor reference plane
- Automatic grid snapping setup

#### Tile Snapper
**Purpose:** Snap objects to precise grid positions
**Modes:**
- Origin snapping (default)
- Geometry snapping
**Grid Size:** Configurable

### Wall Building

#### Wall Builder
**Purpose:** Build walls along paths or between points
**Input Sources:**
- Curves (follow path automatically)
- Mesh objects (use bounding box)
- Manual placement

**Features:**
- Automatic wall segment duplication
- Height scaling
- Rotation alignment
- Optional corner pieces
- Auto-connecting segments

**Requirements:** Wall template object

### Room Generation

#### Room Layout Generator
**Purpose:** Generate complete room layouts from templates
**Room Types:**
- Square rooms
- Rectangle rooms
- L-shaped rooms
- Cross-shaped rooms
- Corridors

**Features:**
- Configurable dimensions (in tiles)
- Automatic floor placement
- Wall generation with door openings
- Optional corner pieces
- Template-based system

**Template Requirements:**
- Floor template (single tile)
- Wall template (wall segment)
- Door template (optional)

### Tile Placement

#### Tile Placer
**Purpose:** Intelligent tile placement with auto-alignment
**Features:**
- Template-based placement
- Grid snapping
- Auto-alignment with nearby tiles
- Edge connection detection

**Use Cases:**
- Manual tile placement
- Detail objects
- Prop placement

## Workflow Guide

### Setting Up a Tile-Based Interior

1. **Create Tile Templates**
   - Model your basic tiles (floor, walls, corners, doors)
   - Set origins appropriately (use Origin Adjuster tools)
   - Ensure consistent scale

2. **Setup Grid System**
   - Use "Tile Grid Setup" to create grid
   - Configure grid size to match your tiles
   - Enable floor reference if needed

3. **Build Room Structure**
   - Use "Room Layout Generator" for basic rooms
   - Or manually place walls using "Wall Builder"
   - Create curves for complex wall layouts

4. **Place Details**
   - Use "Tile Placer" for individual tiles
   - Enable auto-alignment for perfect connections
   - Use tile snapper for precise positioning

5. **Optimize for Engine**
   - Apply LOD generation for complex assets
   - Use Vertex Cache Optimizer
   - Calculate tangent space for normal mapping
   - Bake lighting if needed

### Preparing Assets for C++/Vulkan/DirectX

1. **Mesh Optimization**
   - Apply Vertex Cache Optimizer to all assets
   - Generate LODs for complex objects
   - Remove unused vertex data

2. **Shader Preparation**
   - Calculate tangent space for normal-mapped assets
   - Extract shader constants using the tool
   - Ensure consistent vertex formats

3. **Texture Optimization**
   - Check texel density across assets
   - Generate UV atlases where appropriate
   - Bake lighting to vertices for low-poly assets

4. **Final Export**
   - Use Engine-Specific Optimizer
   - Apply all modifiers
   - Clean up unused data
   - Verify naming conventions

## Best Practices

### Tile-Based Design
- Keep tile sizes consistent (1m, 2m, etc.)
- Use power-of-2 dimensions for textures
- Design tiles to connect seamlessly
- Create corner and edge variants

### Performance Optimization
- Use Vertex Cache Optimizer on all final assets
- Generate LODs for objects >500 polygons
- Keep vertex attributes minimal
- Use texture atlasing to reduce draw calls

### Shader Integration
- Always calculate tangent space for normal mapping
- Extract material properties as shader constants
- Use vertex color baking for ambient occlusion
- Ensure consistent vertex layouts

## Technical Details

### Supported Blender Versions
- Blender 3.0 and later
- Tested on Blender 3.6 LTS and 4.0+

### Performance Considerations
- Vertex Cache Optimizer: O(n log n) complexity
- LOD Generator: Linear scaling with object count
- UV Atlas Generator: Depends on chart count
- All tools support multi-object processing

### Integration with C++ Projects
- Shader constants output in console-ready format
- Material properties extracted as float arrays
- Vertex data optimized for GPU buffers
- Naming conventions match common engine standards

## Troubleshooting

### Common Issues

**"No mesh objects selected"**
- Ensure you have mesh objects selected
- Check that objects are visible and selectable

**"Object needs UV maps"**
- Add UV maps before using UV-related tools
- Use Blender's UV unwrapping tools first

**"Please select template object"**
- Template objects must be selected in tool properties
- Templates should be properly prepared with origins

**Performance Issues**
- Use tools on smaller selections for complex operations
- Ensure sufficient system memory for large datasets
- Close other memory-intensive applications

### Tips for Best Results

1. **Prepare templates first** - ensure all templates have correct origins and scales
2. **Work in small sections** - process complex scenes in parts
3. **Save frequently** - some operations can't be undone
4. **Test on simple objects** - verify tool behavior before complex use
5. **Monitor console output** - important information is displayed there

## Version History

### Version 2.0.0
- Complete toolkit redesign
- Added tile-based interior tools
- Enhanced mesh optimization algorithms
- Improved shader pipeline integration
- Added engine-specific export options

### Version 1.0.0
- Initial release with basic game dev tools
- Vertex cache optimization
- LOD generation
- Basic shader preparation

## Support and Updates

For issues, feature requests, or questions:
- Check the troubleshooting section first
- Verify you're using the latest version
- Ensure Blender compatibility (3.0+)

---

**AEON Engine Development Team**
*Professional tools for professional game developers*

## Special Thanks

To the C++ game development community for feedback and testing:
- Vulkan/DirectX developers
- Shader programmers
- Indie game studios
- Technical artists

This toolkit was built by developers, for developers.