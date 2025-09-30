#!/usr/bin/env python3
"""
NEONTECH Model Generator - Blender Python Script
Automated generation of 3D models following NEONTECH naming convention

Usage: Run in Blender's scripting workspace or via command line:
blender --background --python neontech_model_generator.py
"""

import bpy
import bmesh
import os
import mathutils
from pathlib import Path

# NEONTECH Configuration
NEONTECH_CONFIG = {
    "GRID_SIZE": 1.0,  # 1 meter grid
    "EXPORT_PATH": "C:/Users/chris/repos/neontech/assets/models/",
    "TEXTURE_PATH": "C:/Users/chris/repos/neontech/assets/textures/",
    "DEFAULT_MATERIAL": "TerrazzoVenetianMatteWhite001",
    "FLOOR_MATERIAL": "TilesOnyxOpaloBlack001"
}

class NeontechModelGenerator:
    """NEONTECH 3D Model Generator using Blender Python API"""
    
    def __init__(self):
        self.clear_scene()
        self.setup_materials()
    
    def clear_scene(self):
        """Clear all mesh objects from scene"""
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False, confirm=False)
        
    def setup_materials(self):
        """Create standard NEONTECH PBR materials"""
        materials = {
            'white_terrazzo': (0.85, 0.87, 0.90, 1.0),    # Light walls
            'black_onyx': (0.15, 0.18, 0.22, 1.0),        # Dark floors  
            'gunmetal': (0.50, 0.55, 0.62, 1.0),          # Equipment
            'steel': (0.70, 0.70, 0.75, 1.0),             # Infrastructure
            'glass': (0.95, 0.95, 0.95, 0.3),             # Transparent
        }
        
        for name, color in materials.items():
            mat = bpy.data.materials.new(name=f"NEONTECH_{name}")
            mat.use_nodes = True
            
            # Set up PBR material
            bsdf = mat.node_tree.nodes["Principled BSDF"]
            bsdf.inputs[0].default_value = color  # Base Color
            
            if name == 'steel' or name == 'gunmetal':
                bsdf.inputs[4].default_value = 1.0    # Metallic
                bsdf.inputs[7].default_value = 0.2    # Roughness (shiny)
            elif name == 'glass':
                bsdf.inputs[15].default_value = 0.0   # Transmission (transparent)
            else:
                bsdf.inputs[4].default_value = 0.0    # Metallic (non-metal)
                bsdf.inputs[7].default_value = 0.7    # Roughness (matte)

    def create_floor_tile(self, material_variant="WHITE"):
        """Create 1x1 meter floor tile"""
        bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))
        obj = bpy.context.active_object
        obj.scale = (0.5, 0.5, 0.05)  # 1x1x0.1m floor tile
        obj.name = f"FLOOR_BASIC_{material_variant}_1X1"
        
        # Apply material
        mat_name = "NEONTECH_black_onyx" if material_variant == "BLACK" else "NEONTECH_white_terrazzo"
        if mat_name in bpy.data.materials:
            obj.data.materials.append(bpy.data.materials[mat_name])
        
        return obj
    
    def create_wall_tile(self, material_variant="WHITE"):
        """Create 1x1x1 meter wall cube"""
        bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))
        obj = bpy.context.active_object
        obj.scale = (0.5, 0.5, 0.5)  # 1x1x1m cube
        obj.name = f"WALL_BASIC_{material_variant}_1X1"
        
        # Apply material
        mat_name = "NEONTECH_white_terrazzo" if material_variant == "WHITE" else f"NEONTECH_{material_variant.lower()}"
        if mat_name in bpy.data.materials:
            obj.data.materials.append(bpy.data.materials[mat_name])
        
        return obj
    
    def create_door_frame(self, width=1, height=2):
        """Create door frame with opening"""
        # Create door frame using bmesh
        bm = bmesh.new()
        
        # Create outer rectangle
        bmesh.ops.create_cube(bm, size=2.0)
        
        # Scale to door size
        bmesh.ops.scale(bm, vec=(width/2, 0.1, height/2), verts=bm.verts)
        
        # Create door opening (boolean subtract)
        bmesh.ops.inset_region(bm, faces=bm.faces, thickness=0.1, depth=0.0)
        
        # Create mesh
        mesh = bpy.data.meshes.new(f"DOOR_BASIC_METAL_{width}X{height}")
        bm.to_mesh(mesh)
        bm.free()
        
        obj = bpy.data.objects.new(f"DOOR_BASIC_METAL_{width}X{height}", mesh)
        bpy.context.collection.objects.link(obj)
        
        return obj
    
    def create_equipment_rack(self, width=1, height=2):
        """Create server/equipment rack"""
        bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))
        obj = bpy.context.active_object
        obj.scale = (width/2, 0.4, height/2)  # Standard rack depth
        obj.name = f"EQUIP_SERVER_RACK_{width}X{height}"
        
        # Apply gunmetal material
        if "NEONTECH_gunmetal" in bpy.data.materials:
            obj.data.materials.append(bpy.data.materials["NEONTECH_gunmetal"])
        
        return obj
    
    def setup_uv_mapping(self, obj):
        """Set up proper UV mapping for tiling textures"""
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.uv.smart_project(angle_limit=66, island_margin=0.02)
        bpy.ops.object.mode_set(mode='OBJECT')
    
    def export_model(self, obj, category="building"):
        """Export model with proper naming and directory structure"""
        # Ensure export directory exists
        export_dir = Path(NEONTECH_CONFIG["EXPORT_PATH"]) / category
        export_dir.mkdir(parents=True, exist_ok=True)
        
        # Select only this object
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        
        # Export as OBJ with materials
        filepath = export_dir / f"{obj.name}.obj"
        bpy.ops.export_scene.obj(
            filepath=str(filepath),
            use_selection=True,
            use_materials=True,
            use_uvs=True,
            use_normals=True,
            use_smooth_groups=True
        )
        
        print(f"Exported: {filepath}")
    
    def batch_create_basic_set(self):
        """Create basic building block set"""
        models = []
        
        # Basic floor tiles
        floor_black = self.create_floor_tile("BLACK")
        self.setup_uv_mapping(floor_black)
        models.append((floor_black, "building/floors"))
        
        floor_white = self.create_floor_tile("WHITE") 
        floor_white.location.x = 2.0  # Offset to avoid overlap
        self.setup_uv_mapping(floor_white)
        models.append((floor_white, "building/floors"))
        
        # Basic wall tiles
        wall_white = self.create_wall_tile("WHITE")
        wall_white.location.x = 4.0
        self.setup_uv_mapping(wall_white)
        models.append((wall_white, "building/walls"))
        
        wall_gunmetal = self.create_wall_tile("GUNMETAL")
        wall_gunmetal.location.x = 6.0
        self.setup_uv_mapping(wall_gunmetal)
        models.append((wall_gunmetal, "building/walls"))
        
        # Equipment racks
        server_rack = self.create_equipment_rack(1, 2)
        server_rack.location.x = 8.0
        self.setup_uv_mapping(server_rack)
        models.append((server_rack, "equipment/military"))
        
        # Export all models
        for obj, category in models:
            self.export_model(obj, category)
        
        print(f"Created {len(models)} basic NEONTECH building blocks")

def create_neontech_workspace():
    """Set up Blender workspace for NEONTECH model creation"""
    # Set units to metric (meters)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.length_unit = 'METERS'
    bpy.context.scene.unit_settings.scale_length = 1.0
    
    # Set grid to 1-meter spacing
    bpy.context.space_data.overlay.grid_scale = 1.0
    
    # Set viewport shading to material preview
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.shading.type = 'MATERIAL'

# Main execution
if __name__ == "__main__":
    print("NEONTECH Model Generator - Starting...")
    
    # Setup workspace
    create_neontech_workspace()
    
    # Create generator instance
    generator = NeontechModelGenerator()
    
    # Generate basic model set
    generator.batch_create_basic_set()
    
    print("NEONTECH Model Generator - Complete!")
    print("Models exported to: " + NEONTECH_CONFIG["EXPORT_PATH"])