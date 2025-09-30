import bpy
import bmesh
from mathutils import Vector

def setup_object_for_game(obj=None):
    """
    Prepares an object for game development by:
    1. Moving the origin point to the bottom center of the object
    2. Moving the object up so it sits at floor level (Z=0)

    Args:
        obj: The object to process. If None, uses the active object.
    """

    # Use active object if none specified
    if obj is None:
        obj = bpy.context.active_object

    if obj is None:
        print("No object selected or provided")
        return False

    if obj.type != 'MESH':
        print(f"Object '{obj.name}' is not a mesh object")
        return False

    # Store original location
    original_location = obj.location.copy()

    # Enter Edit mode to access mesh data
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')

    # Create bmesh representation
    bm = bmesh.from_mesh(obj.data)

    # Get world matrix for transformations
    world_matrix = obj.matrix_world

    # Calculate bounding box in world coordinates
    world_coords = [world_matrix @ v.co for v in bm.verts]

    if not world_coords:
        print(f"Object '{obj.name}' has no vertices")
        bpy.ops.object.mode_set(mode='OBJECT')
        return False

    # Find the lowest Z coordinate (bottom of object)
    min_z = min(coord.z for coord in world_coords)

    # Find center X and Y coordinates
    min_x = min(coord.x for coord in world_coords)
    max_x = max(coord.x for coord in world_coords)
    min_y = min(coord.y for coord in world_coords)
    max_y = max(coord.y for coord in world_coords)

    center_x = (min_x + max_x) / 2
    center_y = (min_y + max_y) / 2

    # Calculate where the origin should be (bottom center in world space)
    target_origin_world = Vector((center_x, center_y, min_z))

    bm.free()
    bpy.ops.object.mode_set(mode='OBJECT')

    # Move origin to bottom center
    # First, we need to move the object so the target point is at world origin
    offset = target_origin_world - Vector((0, 0, 0))

    # Move all vertices by the negative offset (in object space)
    world_to_local = obj.matrix_world.inverted()
    local_offset = world_to_local @ offset - world_to_local @ Vector((0, 0, 0))

    # Enter Edit mode to move vertices
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.transform.translate(value=(-local_offset.x, -local_offset.y, -local_offset.z))
    bpy.ops.object.mode_set(mode='OBJECT')

    # Update the object's location to compensate
    obj.location = target_origin_world

    # Now move the object up so it sits on the floor (Z=0)
    # The bottom of the object should now be at the origin, so we just move it up
    obj.location.z = 0

    print(f"✅ Object '{obj.name}' prepared for game development:")
    print(f"   - Origin moved to bottom center")
    print(f"   - Object positioned at floor level")
    print(f"   - Final location: ({obj.location.x:.3f}, {obj.location.y:.3f}, {obj.location.z:.3f})")

    return True

def setup_all_selected_objects():
    """Setup all currently selected objects for game development."""
    selected_objects = [obj for obj in bpy.context.selected_objects if obj.type == 'MESH']

    if not selected_objects:
        print("No mesh objects selected")
        return

    success_count = 0
    for obj in selected_objects:
        if setup_object_for_game(obj):
            success_count += 1

    print(f"\n🎮 Game setup complete: {success_count}/{len(selected_objects)} objects processed")

def setup_active_object():
    """Setup the currently active object for game development."""
    return setup_object_for_game()

# Operator classes for Blender UI integration
class OBJECT_OT_setup_for_game(bpy.types.Operator):
    """Setup selected objects for game development"""
    bl_idname = "object.setup_for_game"
    bl_label = "Setup for Game"
    bl_description = "Move origin to bottom and position at floor level"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        setup_all_selected_objects()
        return {'FINISHED'}

class OBJECT_OT_setup_active_for_game(bpy.types.Operator):
    """Setup active object for game development"""
    bl_idname = "object.setup_active_for_game"
    bl_label = "Setup Active for Game"
    bl_description = "Move origin to bottom and position at floor level (active object only)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if setup_active_object():
            self.report({'INFO'}, "Object setup for game development")
        else:
            self.report({'WARNING'}, "Failed to setup object")
        return {'FINISHED'}

# Panel for easy access in the UI
class VIEW3D_PT_game_object_setup(bpy.types.Panel):
    """Panel for game object setup tools"""
    bl_label = "Game Object Setup"
    bl_idname = "VIEW3D_PT_game_object_setup"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Game Dev"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Prepare objects for games:")
        col.operator("object.setup_active_for_game", text="Setup Active Object")
        col.operator("object.setup_for_game", text="Setup All Selected")

        col.separator()
        col.label(text="This will:")
        col.label(text="• Move origin to bottom center")
        col.label(text="• Position at floor level (Z=0)")

# Registration functions
def register():
    bpy.utils.register_class(OBJECT_OT_setup_for_game)
    bpy.utils.register_class(OBJECT_OT_setup_active_for_game)
    bpy.utils.register_class(VIEW3D_PT_game_object_setup)

    # Add to Object menu
    bpy.types.VIEW3D_MT_object.append(menu_func)

def unregister():
    bpy.utils.unregister_class(OBJECT_OT_setup_for_game)
    bpy.utils.unregister_class(OBJECT_OT_setup_active_for_game)
    bpy.utils.unregister_class(VIEW3D_PT_game_object_setup)

    bpy.types.VIEW3D_MT_object.remove(menu_func)

def menu_func(self, context):
    self.layout.separator()
    self.layout.operator("object.setup_for_game")
    self.layout.operator("object.setup_active_for_game")

# Auto-register if run as script
if __name__ == "__main__":
    register()

    # Example usage: setup the active object
    print("🎮 Game Object Setup Script Loaded")
    print("Usage:")
    print("  - Select object(s) and run setup_all_selected_objects()")
    print("  - Or use setup_active_object() for just the active object")
    print("  - Or use the operators in the Object menu or Game Dev panel")

    # Uncomment to auto-setup active object when script runs:
    # setup_active_object()