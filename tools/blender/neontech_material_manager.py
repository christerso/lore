#!/usr/bin/env python3
"""
NEONTECH Material Manager - Automated PBR Material Setup
Handles automatic material creation, texture mapping, and optimization

Features:
- Automatic PBR material setup from texture folders
- Batch material application to models
- Material validation and optimization
- Export material definitions for NEONTECH engine
"""

import bpy
import os
import json
from pathlib import Path

class NeontechMaterialManager:
    """Professional material management for NEONTECH models"""
    
    def __init__(self, texture_path=None):
        self.texture_path = texture_path or "C:/Users/chris/repos/neontech/assets/textures/"
        self.materials_created = []
    
    def scan_texture_sets(self):
        """Scan texture directory for PBR texture sets"""
        texture_dir = Path(self.texture_path)
        texture_sets = {}
        
        if not texture_dir.exists():
            print(f"Warning: Texture directory not found: {texture_dir}")
            return texture_sets
        
        # Look for PBR texture sets (COL, NRM, GLOSS, REFL, AO, DISP)
        for file in texture_dir.glob("*_COL_*.jpg"):
            base_name = file.name.replace("_COL_2K.jpg", "")
            
            texture_set = {
                'albedo': str(file),
                'normal': None,
                'roughness': None, 
                'metallic': None,
                'ao': None,
                'displacement': None
            }
            
            # Find related texture maps
            for suffix, key in [
                ("_NRM_2K.png", "normal"),
                ("_NRM_2K.jpg", "normal"), 
                ("_GLOSS_2K.jpg", "roughness"),
                ("_REFL_2K.jpg", "metallic"),
                ("_AO_2K.jpg", "ao"),
                ("_DISP_2K.jpg", "displacement")
            ]:
                texture_file = texture_dir / f"{base_name}{suffix}"
                if texture_file.exists():
                    texture_set[key] = str(texture_file)
            
            texture_sets[base_name] = texture_set
        
        print(f"Found {len(texture_sets)} PBR texture sets")
        return texture_sets
    
    def create_pbr_material(self, material_name, texture_set):
        """Create complete PBR material from texture set"""
        # Create material
        mat = bpy.data.materials.new(name=f"NEONTECH_PBR_{material_name}")
        mat.use_nodes = True
        
        # Clear default nodes
        mat.node_tree.nodes.clear()
        
        # Create nodes
        bsdf = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        
        # Connect BSDF to output
        mat.node_tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
        
        # Position nodes
        bsdf.location = (0, 0)
        output.location = (400, 0)
        
        # Add texture nodes
        if texture_set['albedo']:
            albedo_tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            albedo_tex.image = bpy.data.images.load(texture_set['albedo'])
            albedo_tex.location = (-400, 200)
            mat.node_tree.links.new(albedo_tex.outputs['Color'], bsdf.inputs['Base Color'])
        
        if texture_set['normal']:
            normal_tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            normal_tex.image = bpy.data.images.load(texture_set['normal'])
            normal_tex.image.colorspace_settings.name = 'Non-Color'
            normal_tex.location = (-400, 0)
            
            normal_map = mat.node_tree.nodes.new('ShaderNodeNormalMap')
            normal_map.location = (-200, 0)
            mat.node_tree.links.new(normal_tex.outputs['Color'], normal_map.inputs['Color'])
            mat.node_tree.links.new(normal_map.outputs['Normal'], bsdf.inputs['Normal'])
        
        if texture_set['roughness']:
            rough_tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            rough_tex.image = bpy.data.images.load(texture_set['roughness'])
            rough_tex.image.colorspace_settings.name = 'Non-Color'
            rough_tex.location = (-400, -200)
            mat.node_tree.links.new(rough_tex.outputs['Color'], bsdf.inputs['Roughness'])
        
        if texture_set['metallic']:
            metal_tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            metal_tex.image = bpy.data.images.load(texture_set['metallic'])
            metal_tex.image.colorspace_settings.name = 'Non-Color'
            metal_tex.location = (-400, -400)
            mat.node_tree.links.new(metal_tex.outputs['Color'], bsdf.inputs['Metallic'])
        
        self.materials_created.append(mat.name)
        return mat
    
    def auto_assign_materials(self):
        """Automatically assign materials based on object names"""
        material_mapping = {
            'FLOOR_ONYX': 'TilesOnyxOpaloBlack001',
            'FLOOR_TERRAZZO': 'TerrazzoVenetianMatteWhite001', 
            'WALL_WHITE': 'TerrazzoVenetianMatteWhite001',
            'WALL_BASIC': 'TerrazzoVenetianMatteWhite001'
        }
        
        for obj in bpy.context.scene.objects:
            if obj.type == 'MESH':
                for name_pattern, material_name in material_mapping.items():
                    if name_pattern in obj.name:
                        mat_name = f"NEONTECH_PBR_{material_name}"
                        if mat_name in bpy.data.materials:
                            if not obj.data.materials:
                                obj.data.materials.append(bpy.data.materials[mat_name])
                            break
    
    def export_material_definitions(self, filepath=None):
        """Export material definitions for NEONTECH engine"""
        if not filepath:
            filepath = Path(self.texture_path).parent / "materials.json"
        
        material_data = {}
        
        for mat_name in self.materials_created:
            mat = bpy.data.materials.get(mat_name)
            if mat and mat.use_nodes:
                bsdf = mat.node_tree.nodes.get("Principled BSDF")
                if bsdf:
                    material_data[mat_name] = {
                        'base_color': list(bsdf.inputs['Base Color'].default_value),
                        'metallic': bsdf.inputs['Metallic'].default_value,
                        'roughness': bsdf.inputs['Roughness'].default_value,
                        'normal_strength': 1.0
                    }
        
        with open(filepath, 'w') as f:
            json.dump(material_data, f, indent=2)
        
        print(f"Exported {len(material_data)} material definitions to {filepath}")
    
    def validate_materials(self):
        """Validate materials meet NEONTECH standards"""
        issues = []
        
        for obj in bpy.context.scene.objects:
            if obj.type == 'MESH':
                if not obj.data.materials:
                    issues.append(f"Object {obj.name} has no materials")
                
                # Check UV mapping
                if not obj.data.uv_layers:
                    issues.append(f"Object {obj.name} has no UV mapping")
        
        if issues:
            print("Material Validation Issues:")
            for issue in issues:
                print(f"  - {issue}")
        else:
            print("All materials pass validation!")
        
        return len(issues) == 0

def setup_neontech_materials():
    """Main function to set up all NEONTECH materials"""
    manager = NeontechMaterialManager()
    
    # Scan for texture sets
    texture_sets = manager.scan_texture_sets()
    
    # Create PBR materials
    for name, textures in texture_sets.items():
        material = manager.create_pbr_material(name, textures)
        print(f"Created PBR material: {material.name}")
    
    # Auto-assign to objects
    manager.auto_assign_materials()
    
    # Validate everything
    manager.validate_materials()
    
    # Export material definitions
    manager.export_material_definitions()

if __name__ == "__main__":
    setup_neontech_materials()