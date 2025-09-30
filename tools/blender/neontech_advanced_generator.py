#!/usr/bin/env python3
"""
NEONTECH Advanced Model Generator - Complex Equipment & Assemblies
Specialized Blender script for creating detailed military/lab equipment

Features:
- Server racks with individual blade servers and LED indicators
- Workstation assemblies with monitors, keyboards, chairs
- Complex multi-part equipment with proper materials
- Automatic LOD generation
- Batch export with optimization
"""

import bpy
import bmesh
import mathutils
from mathutils import Vector
import random

class NeontechAdvancedGenerator:
    """Advanced model generator for complex NEONTECH equipment"""
    
    def __init__(self):
        self.setup_advanced_materials()
    
    def setup_advanced_materials(self):
        """Create specialized materials for equipment"""
        materials = {
            # Equipment materials
            'server_chassis': (0.2, 0.2, 0.25, 1.0),      # Dark server housing
            'led_green': (0.1, 1.0, 0.1, 1.0),            # Active LED
            'led_red': (1.0, 0.1, 0.1, 1.0),              # Error LED
            'led_blue': (0.1, 0.5, 1.0, 1.0),             # Status LED
            'screen_black': (0.05, 0.05, 0.05, 1.0),      # Monitor screen
            'screen_glow': (0.3, 0.6, 1.0, 1.0),          # Active screen
            'cable_black': (0.1, 0.1, 0.1, 1.0),          # Cable insulation
            'metal_brushed': (0.6, 0.6, 0.65, 1.0),       # Brushed aluminum
        }
        
        for name, color in materials.items():
            mat = bpy.data.materials.new(name=f"NEONTECH_ADV_{name}")
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes["Principled BSDF"]
            bsdf.inputs[0].default_value = color
            
            # Set material properties
            if 'led_' in name:
                bsdf.inputs[17].default_value = 2.0  # Emission strength
                bsdf.inputs[18].default_value = color[:3] + (1.0,)  # Emission color
            elif 'metal' in name or 'steel' in name:
                bsdf.inputs[4].default_value = 0.8   # Metallic
                bsdf.inputs[7].default_value = 0.1   # Roughness
            elif 'screen' in name and 'glow' in name:
                bsdf.inputs[17].default_value = 1.5  # Emission
    
    def create_server_rack(self, x=0, y=0, z=1):
        """Create detailed server rack with individual components"""
        collection = bpy.data.collections.new("ServerRack")
        bpy.context.scene.collection.children.link(collection)
        
        # Main chassis (2m tall, standard 19-inch rack)
        bpy.ops.mesh.primitive_cube_add(size=2, location=(x, y, z))
        chassis = bpy.context.active_object
        chassis.scale = (0.3, 0.5, 1.0)  # 0.6×1.0×2.0m rack
        chassis.name = "ServerRack_Chassis"
        chassis.data.materials.append(bpy.data.materials.get("NEONTECH_ADV_server_chassis"))
        collection.objects.link(chassis)
        bpy.context.scene.collection.objects.unlink(chassis)
        
        # Create 8 blade servers
        for slot in range(8):
            server_y = -0.4 + (slot * 0.1)  # Stack servers
            server_z = z - 0.7 + (slot * 0.2)  # Vertical spacing
            
            # Server blade
            bpy.ops.mesh.primitive_cube_add(size=2, location=(x, y + server_y, server_z))
            server = bpy.context.active_object
            server.scale = (0.25, 0.05, 0.08)  # Thin blade server
            server.name = f"ServerRack_Blade_{slot:02d}"
            server.data.materials.append(bpy.data.materials.get("NEONTECH_gunmetal"))
            collection.objects.link(server)
            bpy.context.scene.collection.objects.unlink(server)
            
            # LED indicators (2 per server)
            for led_idx in range(2):
                led_x = x + 0.15 + (led_idx * 0.1)
                bpy.ops.mesh.primitive_uv_sphere_add(
                    radius=0.02, 
                    location=(led_x, y + server_y + 0.03, server_z)
                )
                led = bpy.context.active_object
                led.name = f"ServerRack_LED_{slot:02d}_{led_idx}"
                
                # Random LED color for status indication
                led_material = random.choice([
                    "NEONTECH_ADV_led_green",
                    "NEONTECH_ADV_led_blue", 
                    "NEONTECH_ADV_led_red"
                ])
                led.data.materials.append(bpy.data.materials.get(led_material))
                collection.objects.link(led)
                bpy.context.scene.collection.objects.unlink(led)
        
        return collection
    
    def create_workstation_assembly(self, x=0, y=0, z=0):
        """Create complete workstation with monitors, desk, chair"""
        collection = bpy.data.collections.new("Workstation")
        bpy.context.scene.collection.children.link(collection)
        
        # Desk
        bpy.ops.mesh.primitive_cube_add(size=2, location=(x, y, z + 0.4))
        desk = bpy.context.active_object
        desk.scale = (1.0, 0.4, 0.4)  # 2×0.8×0.8m desk
        desk.name = "Workstation_Desk"
        desk.data.materials.append(bpy.data.materials.get("NEONTECH_ADV_metal_brushed"))
        collection.objects.link(desk)
        bpy.context.scene.collection.objects.unlink(desk)
        
        # Monitors (dual setup)
        for monitor_idx in range(2):
            monitor_x = x - 0.3 + (monitor_idx * 0.6)
            
            # Monitor base
            bpy.ops.mesh.primitive_cube_add(size=2, location=(monitor_x, y + 0.3, z + 0.8))
            monitor = bpy.context.active_object
            monitor.scale = (0.15, 0.02, 0.12)  # Thin monitor
            monitor.name = f"Workstation_Monitor_{monitor_idx:02d}"
            monitor.data.materials.append(bpy.data.materials.get("NEONTECH_ADV_screen_black"))
            collection.objects.link(monitor)
            bpy.context.scene.collection.objects.unlink(monitor)
            
            # Screen glow (when active)
            bpy.ops.mesh.primitive_plane_add(size=2, location=(monitor_x, y + 0.32, z + 0.8))
            screen = bpy.context.active_object
            screen.scale = (0.13, 0.10, 1.0)  # Screen area
            screen.name = f"Workstation_Screen_{monitor_idx:02d}"
            screen.data.materials.append(bpy.data.materials.get("NEONTECH_ADV_screen_glow"))
            collection.objects.link(screen)
            bpy.context.scene.collection.objects.unlink(screen)
        
        # Chair
        bpy.ops.mesh.primitive_cube_add(size=2, location=(x, y - 0.8, z + 0.25))
        chair = bpy.context.active_object
        chair.scale = (0.3, 0.3, 0.25)  # Office chair
        chair.name = "Workstation_Chair"
        chair.data.materials.append(bpy.data.materials.get("NEONTECH_gunmetal"))
        collection.objects.link(chair)
        bpy.context.scene.collection.objects.unlink(chair)
        
        return collection
    
    def create_laboratory_setup(self, x=0, y=0, z=0):
        """Create laboratory workbench with equipment"""
        collection = bpy.data.collections.new("LabSetup")
        bpy.context.scene.collection.children.link(collection)
        
        # Lab bench
        bpy.ops.mesh.primitive_cube_add(size=2, location=(x, y, z + 0.45))
        bench = bpy.context.active_object
        bench.scale = (2.0, 0.5, 0.45)  # 4×1×0.9m lab bench
        bench.name = "Lab_Workbench"
        bench.data.materials.append(bpy.data.materials.get("NEONTECH_white_terrazzo"))
        collection.objects.link(bench)
        bpy.context.scene.collection.objects.unlink(bench)
        
        # Equipment on bench
        equipment_positions = [
            (-1.5, 0.3, 0.9, "Microscope"),
            (-0.5, 0.3, 0.9, "Analyzer"), 
            (0.5, 0.3, 0.9, "Centrifuge"),
            (1.5, 0.3, 0.9, "Computer")
        ]
        
        for eq_x, eq_y, eq_z, name in equipment_positions:
            bpy.ops.mesh.primitive_cube_add(size=2, location=(x + eq_x, y + eq_y, z + eq_z))
            equipment = bpy.context.active_object
            equipment.scale = (0.15, 0.15, 0.15)  # Small equipment
            equipment.name = f"Lab_{name}"
            equipment.data.materials.append(bpy.data.materials.get("NEONTECH_gunmetal"))
            collection.objects.link(equipment)
            bpy.context.scene.collection.objects.unlink(equipment)
        
        return collection
    
    def apply_lod_optimization(self, obj, lod_levels=[1.0, 0.5, 0.25]):
        """Create multiple LOD levels for performance"""
        base_name = obj.name
        
        for idx, factor in enumerate(lod_levels):
            # Duplicate object
            obj_copy = obj.copy()
            obj_copy.data = obj.data.copy()
            obj_copy.name = f"{base_name}_LOD{idx}"
            bpy.context.collection.objects.link(obj_copy)
            
            # Apply decimation for lower LOD levels
            if factor < 1.0:
                bpy.context.view_layer.objects.active = obj_copy
                decimate = obj_copy.modifiers.new(name="Decimate", type='DECIMATE')
                decimate.ratio = factor
                bpy.ops.object.modifier_apply(modifier="Decimate")
    
    def batch_export_all(self):
        """Export all models in scene with proper organization"""
        for obj in bpy.context.scene.objects:
            if obj.type == 'MESH' and not obj.name.startswith("Camera"):
                # Determine category from name
                if obj.name.startswith("FLOOR_"):
                    category = "building/floors"
                elif obj.name.startswith("WALL_"):
                    category = "building/walls"
                elif obj.name.startswith("EQUIP_"):
                    category = "equipment/military"
                elif obj.name.startswith("FURNITURE_"):
                    category = "furniture"
                else:
                    category = "misc"
                
                self.export_model(obj, category)

# Utility functions for manual use
def create_custom_model(model_type, variant, size_x=1, size_y=1, size_z=1):
    """Create custom model with NEONTECH naming convention"""
    generator = NeontechAdvancedGenerator()
    
    bpy.ops.mesh.primitive_cube_add(size=2)
    obj = bpy.context.active_object
    obj.scale = (size_x/2, size_y/2, size_z/2)
    obj.name = f"{model_type}_{variant}_{size_x}X{size_y}"
    
    generator.setup_uv_mapping(obj)
    return obj

def setup_pbr_material(obj, albedo_path, normal_path=None, roughness_path=None):
    """Set up PBR material with texture maps"""
    if not obj.data.materials:
        mat = bpy.data.materials.new(name=f"{obj.name}_Material")
        mat.use_nodes = True
        obj.data.materials.append(mat)
        
        # Set up PBR nodes
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        
        # Albedo texture
        if os.path.exists(albedo_path):
            tex_image = mat.node_tree.nodes.new('ShaderNodeTexImage')
            tex_image.image = bpy.data.images.load(albedo_path)
            mat.node_tree.links.new(bsdf.inputs['Base Color'], tex_image.outputs['Color'])

def batch_optimize_models():
    """Optimize all models in scene for game use"""
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            # Ensure proper normals
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.normals_make_consistent(inside=False)
            bpy.ops.object.mode_set(mode='OBJECT')
            
            # Apply scale/rotation
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

if __name__ == "__main__":
    print("NEONTECH Advanced Generator - Creating detailed equipment...")
    
    generator = NeontechAdvancedGenerator()
    
    # Create server rack assembly
    server_collection = generator.create_server_rack(0, 0, 1)
    print("Created detailed server rack with LEDs")
    
    # Create workstation assembly  
    workstation_collection = generator.create_workstation_assembly(5, 0, 0)
    print("Created workstation with dual monitors")
    
    # Create lab setup
    lab_collection = generator.create_laboratory_setup(10, 0, 0)
    print("Created laboratory workbench with equipment")
    
    # Export all assemblies
    generator.batch_export_all()
    print("Exported all advanced equipment models")