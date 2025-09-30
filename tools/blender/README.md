# NEONTECH Blender Automation Scripts

Professional 3D model creation and processing pipeline for NEONTECH classical rogue-like game.

## 📋 **Quick Start Guide**

### **1. Setup Blender Environment**
```python
# In Blender's scripting workspace, run:
exec(open("tools/blender/neontech_model_generator.py").read())
```

### **2. Generate Basic Building Blocks**
```python
# Creates floor tiles, wall cubes, basic equipment
generator = NeontechModelGenerator()
generator.batch_create_basic_set()
```

### **3. Set Up Materials**
```python
# Automatically creates PBR materials from texture files
exec(open("tools/blender/neontech_material_manager.py").read())
setup_neontech_materials()
```

### **4. Create Advanced Equipment**
```python
# Generates detailed server racks, workstations, lab equipment
exec(open("tools/blender/neontech_advanced_generator.py").read())
```

### **5. Batch Process & Export**
```python
# Optimizes, validates, and exports all models
exec(open("tools/blender/neontech_batch_processor.py").read())
run_neontech_pipeline()
```

---

## 🔧 **Script Reference**

### **neontech_model_generator.py**
**Purpose**: Create basic building blocks (floors, walls, doors, stairs)

**Key Functions**:
- `create_floor_tile(material)` - 1×1m floor tiles
- `create_wall_tile(material)` - 1×1×1m wall cubes  
- `create_door_frame(width, height)` - Door assemblies
- `batch_create_basic_set()` - Generate complete basic set

**Materials**: Automatically applies appropriate PBR materials based on naming

### **neontech_advanced_generator.py**
**Purpose**: Create complex equipment assemblies

**Key Functions**:
- `create_server_rack(x, y, z)` - Server rack with 8 blade servers and LEDs
- `create_workstation_assembly(x, y, z)` - Dual-monitor workstation with chair
- `create_laboratory_setup(x, y, z)` - Lab bench with scientific equipment

**Features**: Multi-part assemblies with proper component relationships

### **neontech_material_manager.py** 
**Purpose**: Automated PBR material creation and management

**Key Functions**:
- `scan_texture_sets()` - Auto-detect PBR texture sets
- `create_pbr_material(name, textures)` - Create complete PBR materials
- `auto_assign_materials()` - Apply materials based on naming convention
- `validate_materials()` - Quality assurance checks

**Supports**: TerrazzoVenetianMatteWhite001, TilesOnyxOpaloBlack001, custom materials

### **neontech_batch_processor.py**
**Purpose**: Production pipeline automation

**Key Functions**:
- `validate_model_standards(obj)` - Check NEONTECH compliance
- `optimize_model(obj)` - Apply optimization standards
- `create_lod_versions(obj)` - Generate LOD0, LOD1, LOD2
- `batch_process_directory(path)` - Process entire directories
- `generate_import_manifest()` - Create JSON manifest for engine

**Output**: Optimized models ready for NEONTECH engine import

---

## 📐 **Model Creation Guidelines**

### **Grid Alignment**
- All models must align to **1-meter grid**
- Use Blender's snap settings: Grid, 1.0 unit increment
- Models should sit on grid intersections

### **Naming Convention** 
- Follow format: `{CATEGORY}_{TYPE}_{VARIANT}_{SIZE}`
- Examples: `WALL_BASIC_WHITE_1X1`, `EQUIP_SERVER_RACK_2X1`

### **Size Standards**
- `1X1` = 1×1 meter footprint (single tile)
- `2X1` = 2×1 meter footprint (double wide)
- `3X3` = 3×3 meter footprint (large equipment)

### **Material Requirements**
- Use PBR workflow: Albedo, Normal, Roughness, Metallic, AO
- Follow NEONTECH material naming convention
- Ensure proper UV mapping (0-1 space, non-overlapping)

---

## 🎮 **Workflow Examples**

### **Creating a Wall Set**
```python
generator = NeontechModelGenerator()

# Create different wall variants
wall_basic = generator.create_wall_tile("WHITE")
wall_damaged = generator.create_wall_tile("DAMAGED") 
wall_steel = generator.create_wall_tile("STEEL")

# Position them
wall_damaged.location.x = 2.0
wall_steel.location.x = 4.0

# Export
for obj in [wall_basic, wall_damaged, wall_steel]:
    generator.export_model(obj, "building/walls")
```

### **Creating Equipment Room**
```python
advanced = NeontechAdvancedGenerator()

# Create server farm
server_rack_1 = advanced.create_server_rack(0, 0, 1)
server_rack_2 = advanced.create_server_rack(2, 0, 1) 

# Create workstations
workstation_1 = advanced.create_workstation_assembly(5, 0, 0)
workstation_2 = advanced.create_workstation_assembly(8, 0, 0)

# Process and export
processor = NeontechBatchProcessor()
run_neontech_pipeline()
```

### **Material Setup**
```python
manager = NeontechMaterialManager()

# Scan textures and create materials
texture_sets = manager.scan_texture_sets()
for name, textures in texture_sets.items():
    manager.create_pbr_material(name, textures)

# Apply to all objects
manager.auto_assign_materials()
manager.validate_materials()
```

---

## 🔍 **Quality Assurance**

### **Automatic Validation**
- **Polygon Count**: <1000 triangles per 1×1 tile
- **UV Mapping**: Complete, non-overlapping UVs
- **Materials**: PBR materials assigned
- **Scale**: Transforms applied
- **Normals**: Consistent face orientation

### **LOD Generation**
- **LOD0**: Full detail (100% polygons)
- **LOD1**: Medium detail (60% polygons) 
- **LOD2**: Low detail (30% polygons)

### **Export Standards**
- **Format**: OBJ with MTL materials
- **Coordinates**: Y-up, Z-forward
- **Scale**: 1 Blender unit = 1 meter
- **Optimization**: Applied modifiers, clean geometry

---

## 🚀 **Command Line Usage**

### **Batch Processing**
```bash
# Process entire directory of .blend files
blender --background --python neontech_batch_processor.py -- --input "source_models/"

# Generate basic set
blender --background --python neontech_model_generator.py

# Update materials from new textures
blender --background --python neontech_material_manager.py
```

### **Integration with NEONTECH Build**
```cmake
# Add custom target in CMakeLists.txt
add_custom_target(generate_models
    COMMAND blender --background --python ${CMAKE_SOURCE_DIR}/tools/blender/neontech_batch_processor.py
    COMMENT "Generating NEONTECH 3D models"
)
```

---

## 📊 **Performance Metrics**

The pipeline tracks:
- **Model Polygon Counts**: Ensures <1000 triangles per tile
- **Texture Memory Usage**: Optimizes for GPU memory
- **Export File Sizes**: Monitors asset bloat
- **Processing Time**: Tracks automation efficiency

**Target Performance**:
- Basic tile: <200 polygons
- Equipment: <800 polygons  
- Complex assembly: <1500 polygons
- Export time: <2 seconds per model

This automation pipeline ensures all NEONTECH models meet professional game development standards while maintaining the classical rogue-like tile-based gameplay!