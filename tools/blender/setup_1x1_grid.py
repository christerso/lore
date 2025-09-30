#!/usr/bin/env python3
"""
NEONTECH 1×1 Meter Grid Setup
Sets up Blender workspace for precise 1-meter tile creation

Run this script first before creating any models to ensure perfect 1×1 meter alignment
"""

import bpy
import bmesh

def setup_neontech_workspace():
    """Configure Blender for precise 1-meter grid work"""
    print("Setting up NEONTECH 1×1 meter workspace...")
    
    # 1. Set units to metric with 1-meter precision
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS' 
    scene.unit_settings.scale_length = 1.0
    
    # 2. Configure viewport grid to 1-meter spacing
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.overlay.show_floor = True
                    space.overlay.show_axis_x = True
                    space.overlay.show_axis_y = True
                    space.overlay.show_axis_z = True
                    space.overlay.grid_scale = 1.0  # 1-meter grid lines
                    space.overlay.grid_subdivisions = 10  # 0.1m subdivisions
    
    # 3. Set snap settings for 1-meter alignment
    scene.tool_settings.use_snap = True
    scene.tool_settings.snap_elements = {'INCREMENT'}  # Snap to grid
    scene.tool_settings.snap_target = 'CENTER'
    scene.tool_settings.use_snap_align_rotation = True
    
    # 4. Set cursor to origin (0,0,0)
    scene.cursor.location = (0.0, 0.0, 0.0)
    
    print("✅ Workspace configured for 1-meter grid precision")

def create_perfect_1x1_cube():
    """Create a perfect 1×1×1 meter reference cube"""
    # Clear existing selection
    bpy.ops.object.select_all(action='DESELECT')
    
    # Add cube primitive
    bpy.ops.mesh.primitive_cube_add(size=2.0, location=(0, 0, 0.5))  # Size 2 = 1m radius = 2m total
    
    cube = bpy.context.active_object
    cube.name = "WALL_BASIC_WHITE_1X1"
    cube.scale = (0.5, 0.5, 0.5)  # Scale down to exactly 1×1×1 meter
    
    # Apply the scale to make it permanent
    bpy.context.view_layer.objects.active = cube
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    
    # Verify dimensions
    dimensions = cube.dimensions
    print(f"✅ Created 1×1×1m cube: {dimensions.x:.2f}×{dimensions.y:.2f}×{dimensions.z:.2f} meters")
    
    return cube

def create_perfect_1x1_floor():
    """Create a perfect 1×1 meter floor tile"""
    # Clear existing selection
    bpy.ops.object.select_all(action='DESELECT')
    
    # Add cube primitive for floor (thin)
    bpy.ops.mesh.primitive_cube_add(size=2.0, location=(2, 0, 0.05))  
    
    floor = bpy.context.active_object
    floor.name = "FLOOR_ONYX_BLACK_1X1"
    floor.scale = (0.5, 0.5, 0.05)  # 1×1×0.1m floor tile
    
    # Apply the scale
    bpy.context.view_layer.objects.active = floor
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    
    dimensions = floor.dimensions
    print(f"✅ Created 1×1m floor: {dimensions.x:.2f}×{dimensions.y:.2f}×{dimensions.z:.2f} meters")
    
    return floor

def validate_1meter_precision():
    """Validate that objects are exactly 1-meter aligned"""
    print("\n🔍 Validating 1-meter precision...")
    
    issues_found = False
    
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            # Check if dimensions are multiples of 1 meter
            dims = obj.dimensions
            
            for axis, size in enumerate([dims.x, dims.y, dims.z]):
                remainder = size % 1.0
                if remainder > 0.01:  # 1cm tolerance
                    print(f"❌ {obj.name} - Axis {axis}: {size:.3f}m (not 1m multiple)")
                    issues_found = True
            
            # Check if location is on grid
            loc = obj.location
            for axis, pos in enumerate([loc.x, loc.y, loc.z]):
                remainder = pos % 1.0
                if remainder > 0.01:  # 1cm tolerance  
                    print(f"❌ {obj.name} - Position {axis}: {pos:.3f}m (not on grid)")
                    issues_found = True
    
    if not issues_found:
        print("✅ All objects are properly aligned to 1-meter grid")
    
    return not issues_found

def create_measurement_guides():
    """Create visual measurement guides"""
    # Create 1-meter measurement cube (wireframe)
    bpy.ops.mesh.primitive_cube_add(size=2.0, location=(4, 0, 0.5))
    guide = bpy.context.active_object
    guide.name = "GUIDE_1METER_REFERENCE"
    guide.scale = (0.5, 0.5, 0.5)
    guide.display_type = 'WIRE'  # Wireframe only
    
    # Lock it so it can't be accidentally moved
    guide.lock_location[0] = True
    guide.lock_location[1] = True  
    guide.lock_location[2] = True
    guide.lock_scale[0] = True
    guide.lock_scale[1] = True
    guide.lock_scale[2] = True
    
    print("✅ Created 1-meter reference guide")

def quick_setup_and_test():
    """Quick setup and create test models"""
    print("🚀 NEONTECH 1×1 Meter Grid Setup - Starting...")
    
    # Clear scene
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False, confirm=False)
    
    # Setup workspace
    setup_neontech_workspace()
    
    # Create reference models
    cube = create_perfect_1x1_cube()
    floor = create_perfect_1x1_floor()
    
    # Create measurement guides
    create_measurement_guides()
    
    # Validate precision
    is_valid = validate_1meter_precision()
    
    if is_valid:
        print("🎯 Perfect! Ready to create 1×1 meter NEONTECH models")
        print("\nNext steps:")
        print("1. Use the reference cube as template")
        print("2. All new objects should align to grid lines") 
        print("3. Use snap settings for precision")
        print("4. Validate with validate_1meter_precision() before export")
    else:
        print("⚠️ Precision issues detected - please fix before continuing")
    
    return is_valid

# Run automatically when imported
if __name__ == "__main__":
    success = quick_setup_and_test()
    
    if success:
        print("\n📐 WORKSPACE READY!")
        print("Your Blender is now configured for precise 1×1 meter NEONTECH models")
        print("The reference cube shows exact 1×1×1 meter dimensions")
    else:
        print("\n⚠️ Setup had issues - please check the console output")