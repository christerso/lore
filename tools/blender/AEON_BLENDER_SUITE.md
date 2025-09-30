# AEON Engine - Complete Blender Tool Suite

Professional 3D asset creation pipeline for the AEON multi-era game engine, combining game-ready object preparation with advanced MCP automation.

## 🎮 **Core Tools Overview**

### **1. Game Object Setup (`game_object_setup.py`)**
**Purpose**: Prepare any 3D object for AEON engine integration
- Moves origin to bottom center for proper placement
- Positions objects at floor level (Z=0)
- Essential for consistent game asset behavior

### **2. Blender MCP Integration (`blender_mcp_addon.py`)**
**Purpose**: Direct Claude-to-Blender automation
- Real-time 3D model generation via Claude commands
- Professional PBR material management
- Automated AEON asset pipeline

### **3. NEONTECH Model Generators**
**Purpose**: Specialized tools for advanced asset creation
- Procedural building blocks and equipment
- Material automation with PBR workflows
- Batch processing and optimization

## 🚀 **Quick Start Guide**

### **Method 1: Basic Game Asset Preparation**
```python
# For preparing existing models for AEON
1. Open Blender, load your model
2. Run game_object_setup.py
3. Click "Setup for Game" in Game Dev panel
4. Export to AEON engine
```

### **Method 2: Advanced MCP Automation**
```python
# For AI-powered model creation
1. Install blender_mcp_addon.py in Blender
2. Connect to Claude via MCP tab
3. Ask Claude to create specific models
4. Automatic AEON-ready export
```

### **Method 3: Professional Pipeline**
```python
# For large-scale asset production
1. Use NEONTECH generators for base models
2. Apply MCP for iterations and refinements
3. Process with game_object_setup for AEON compatibility
4. Batch export optimized assets
```

## 📋 **Installation Options**

### **Option A: Windows Installer**
```bash
cd G:/repos/aeon/tools/blender/
./install_addon.bat
# Automatically finds Blender and installs all tools
```

### **Option B: Manual Installation**
1. **Blender Preferences** → **Add-ons** → **Install...**
2. Select desired scripts:
   - `game_object_setup.py` - Basic game preparation
   - `blender_mcp_addon.py` - MCP automation
   - NEONTECH scripts - Advanced generation
3. Enable checkboxes for installed addons

### **Option C: Script Execution**
1. **Scripting workspace** → **Open** script files
2. **Run Script** to load functions
3. Use via Python console or UI panels

## 🎯 **AEON Engine Integration**

### **Asset Requirements for AEON**
- **Origin Point**: Bottom center for proper placement
- **Floor Alignment**: Objects sit at Z=0 ground level
- **Scale**: 1 Blender unit = 1 meter in AEON
- **Materials**: PBR workflow with proper UV mapping
- **Optimization**: <1000 triangles per 1x1 meter tile

### **AEON Workflow Pipeline**
1. **Create/Import** → 2. **Setup for Game** → 3. **Export** → 4. **AEON Import**

### **File Formats Supported**
- **FBX**: Primary format for AEON engine
- **OBJ**: Legacy support with MTL materials
- **glTF**: Modern standard with PBR materials
- **BLEND**: Source files for iteration

## 🔧 **Tool Details**

### **Game Object Setup**
```python
# Python API
setup_active_object()              # Current object
setup_all_selected_objects()       # All selected
setup_object_for_game(obj)         # Specific object

# UI Access
# 3D Viewport → Game Dev panel → Setup buttons
# Object menu → Setup for Game options
```

### **MCP Blender Addon**
```python
# Claude can directly execute:
create_1x1_wall_block()
create_floor_tile("STONE")
create_server_rack_assembly()
apply_pbr_material("MetalRoughness")
export_aeon_models("/path/to/assets/")
```

### **NEONTECH Generators**
```python
# Professional model creation
generator = NeontechModelGenerator()
generator.batch_create_basic_set()

# Advanced assemblies
advanced = NeontechAdvancedGenerator()
server_rack = advanced.create_server_rack(0, 0, 1)
workstation = advanced.create_workstation_assembly(5, 0, 0)

# Material management
manager = NeontechMaterialManager()
manager.setup_pbr_materials()
manager.auto_assign_materials()
```

## 📐 **AEON Asset Standards**

### **Modeling Guidelines**
- **Grid Alignment**: 1-meter grid for building blocks
- **Polygon Limits**: <1000 triangles per tile
- **UV Mapping**: Complete, non-overlapping 0-1 space
- **Transforms**: Applied scales, rotations, and locations

### **Material Standards**
- **PBR Workflow**: Albedo, Normal, Roughness, Metallic, AO
- **Texture Resolution**: Power-of-2 (512px, 1024px, 2048px)
- **Naming Convention**: Clear, descriptive material names
- **LOD Support**: Multiple detail levels when needed

### **Naming Convention**
```
{CATEGORY}_{TYPE}_{VARIANT}_{SIZE}
Examples:
- BUILDING_WALL_CONCRETE_1X1
- FURNITURE_DESK_METAL_2X1
- EQUIPMENT_SERVER_RACK_3X1
- DECORATION_PLANT_FERN_1X1
```

## 🎮 **Game Development Features**

### **Multi-Era Support**
The tools support AEON's multi-era gameplay:
- **Ancient**: Stone, wood, bronze materials
- **Medieval**: Iron, leather, castle architecture
- **Renaissance**: Advanced stonework, early machinery
- **Industrial**: Steel, steam-powered equipment
- **Modern**: Electronics, plastics, contemporary design
- **Future**: Sci-fi materials, energy-based systems

### **Procedural Variations**
- Material variants (weathered, new, damaged)
- Size variations (1x1, 2x1, 3x3 modular tiles)
- Functional states (powered/unpowered equipment)
- Cultural variations (different civilizations)

### **Performance Optimization**
- Automatic LOD generation (LOD0, LOD1, LOD2)
- Triangle count optimization
- Texture atlas generation
- Occlusion culling support

## 🚀 **Advanced Workflows**

### **Collaborative Development**
```python
# Team workflow with version control
1. Base models via NEONTECH generators
2. Iterations via MCP with Claude
3. Game preparation via setup tools
4. Version control via Git LFS
5. Asset validation pipeline
```

### **Automated Content Creation**
```python
# Large-scale asset generation
for era in ['ancient', 'medieval', 'industrial', 'modern']:
    for building_type in ['residential', 'commercial', 'industrial']:
        create_building_set(era, building_type)
        apply_era_materials(era)
        setup_for_aeon_game()
        export_optimized_assets()
```

### **Quality Assurance Pipeline**
- Automated polygon count checking
- UV mapping validation
- Material assignment verification
- Export format consistency
- Performance benchmarking

## 📊 **Performance Targets**

- **Basic Tiles**: <200 polygons
- **Furniture**: <500 polygons
- **Equipment**: <800 polygons
- **Complex Assemblies**: <1500 polygons
- **Export Time**: <2 seconds per model
- **Texture Memory**: <4MB per material set

This complete suite transforms Blender into a professional AEON game asset creation powerhouse, supporting everything from simple object preparation to advanced AI-powered procedural generation! 🎮✨