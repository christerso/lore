"""
AEON Engine - Origin Point Adjuster Addon
Professional Blender addon for adjusting object origins for game development

Author: AEON Engine Development Team
Version: 1.0.0
License: MIT

This addon provides tools to set object origins optimally for game development:
- Standing Objects: Origin at bottom center (for props, characters, furniture)
- Floor Objects: Origin at top center (for floor tiles, platforms, ceilings)
"""

bl_info = {
    "name": "AEON Origin Adjuster",
    "author": "AEON Engine Development Team",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > AEON Tools",
    "description": "Professional origin adjustment tools for game development",
    "category": "Object",
    "doc_url": "",
    "tracker_url": "",
}

import bpy
import bmesh
from mathutils import Vector
from bpy.props import BoolProperty, EnumProperty
from bpy.types import Panel, Operator


class AEON_OT_AdjustStandingOrigin(Operator):
    """Set origin to bottom center for standing objects (props, characters, furniture)"""
    bl_idname = "aeon.adjust_standing_origin"
    bl_label = "Set Standing Origin"
    bl_description = "Set origin to bottom center - ideal for objects that stand on floors"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.active_object.type == 'MESH'

    def execute(self, context):
        selected_objects = [obj for obj in context.selected_objects if obj.type == 'MESH']

        if not selected_objects:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        processed_count = 0

        for obj in selected_objects:
            try:
                # Store current mode and selection
                original_mode = context.mode
                original_selection = context.selected_objects.copy()

                # Switch to object mode
                if context.mode != 'OBJECT':
                    bpy.ops.object.mode_set(mode='OBJECT')

                # Deselect all objects first
                bpy.ops.object.select_all(action='DESELECT')

                # Select only the current object and make it active
                obj.select_set(True)
                context.view_layer.objects.active = obj

                # Get object's bounding box in world coordinates
                bbox_corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]

                # Calculate bottom center point
                min_z = min(corner.z for corner in bbox_corners)

                # Calculate center X and Y
                min_x = min(corner.x for corner in bbox_corners)
                max_x = max(corner.x for corner in bbox_corners)
                min_y = min(corner.y for corner in bbox_corners)
                max_y = max(corner.y for corner in bbox_corners)

                center_x = (min_x + max_x) / 2.0
                center_y = (min_y + max_y) / 2.0

                # Calculate the cursor position for bottom center
                cursor_location = Vector((center_x, center_y, min_z))

                # Store original cursor position
                original_cursor = context.scene.cursor.location.copy()

                # Set cursor to the desired origin position
                context.scene.cursor.location = cursor_location

                # Use Blender's built-in origin setting
                bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

                # Restore original cursor position
                context.scene.cursor.location = original_cursor

                processed_count += 1

            except Exception as e:
                self.report({'ERROR'}, f"Failed to process {obj.name}: {str(e)}")
                continue

        # Restore original selection
        try:
            bpy.ops.object.select_all(action='DESELECT')
            for selected_obj in original_selection:
                selected_obj.select_set(True)
            if original_selection:
                context.view_layer.objects.active = original_selection[0]
        except:
            pass  # If restoration fails, continue

        if processed_count > 0:
            self.report({'INFO'}, f"Adjusted origin for {processed_count} standing object(s)")
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "Failed to process any objects")
            return {'CANCELLED'}


class AEON_OT_AdjustFloorOrigin(Operator):
    """Set origin to top center for floor objects (tiles, platforms, ceilings)"""
    bl_idname = "aeon.adjust_floor_origin"
    bl_label = "Set Floor Origin"
    bl_description = "Set origin to top center - ideal for floor tiles and platforms"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.active_object.type == 'MESH'

    def execute(self, context):
        selected_objects = [obj for obj in context.selected_objects if obj.type == 'MESH']

        if not selected_objects:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        processed_count = 0

        for obj in selected_objects:
            try:
                # Store current mode and selection
                original_mode = context.mode
                original_selection = context.selected_objects.copy()

                # Switch to object mode
                if context.mode != 'OBJECT':
                    bpy.ops.object.mode_set(mode='OBJECT')

                # Deselect all objects first
                bpy.ops.object.select_all(action='DESELECT')

                # Select only the current object and make it active
                obj.select_set(True)
                context.view_layer.objects.active = obj

                # Get object's bounding box in world coordinates
                bbox_corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]

                # Calculate top center point
                max_z = max(corner.z for corner in bbox_corners)

                # Calculate center X and Y
                min_x = min(corner.x for corner in bbox_corners)
                max_x = max(corner.x for corner in bbox_corners)
                min_y = min(corner.y for corner in bbox_corners)
                max_y = max(corner.y for corner in bbox_corners)

                center_x = (min_x + max_x) / 2.0
                center_y = (min_y + max_y) / 2.0

                # Calculate the cursor position for top center
                cursor_location = Vector((center_x, center_y, max_z))

                # Store original cursor position
                original_cursor = context.scene.cursor.location.copy()

                # Set cursor to the desired origin position
                context.scene.cursor.location = cursor_location

                # Use Blender's built-in origin setting
                bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

                # Restore original cursor position
                context.scene.cursor.location = original_cursor

                processed_count += 1

            except Exception as e:
                self.report({'ERROR'}, f"Failed to process {obj.name}: {str(e)}")
                continue

        # Restore original selection
        try:
            bpy.ops.object.select_all(action='DESELECT')
            for selected_obj in original_selection:
                selected_obj.select_set(True)
            if original_selection:
                context.view_layer.objects.active = original_selection[0]
        except:
            pass  # If restoration fails, continue

        if processed_count > 0:
            self.report({'INFO'}, f"Adjusted origin for {processed_count} floor object(s)")
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "Failed to process any objects")
            return {'CANCELLED'}


class AEON_OT_BatchAdjustOrigins(Operator):
    """Batch adjust origins based on object naming conventions"""
    bl_idname = "aeon.batch_adjust_origins"
    bl_label = "Batch Adjust by Name"
    bl_description = "Auto-detect object type by name and adjust origins accordingly"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        selected_objects = [obj for obj in context.selected_objects if obj.type == 'MESH']

        if not selected_objects:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        floor_keywords = ['floor', 'tile', 'platform', 'ground', 'ceiling', 'roof']
        standing_count = 0
        floor_count = 0

        for obj in selected_objects:
            obj_name_lower = obj.name.lower()
            is_floor = any(keyword in obj_name_lower for keyword in floor_keywords)

            # Set as active object
            context.view_layer.objects.active = obj

            if is_floor:
                # Apply floor origin adjustment
                bpy.ops.aeon.adjust_floor_origin()
                floor_count += 1
            else:
                # Apply standing origin adjustment
                bpy.ops.aeon.adjust_standing_origin()
                standing_count += 1

        message = f"Batch processed: {standing_count} standing objects, {floor_count} floor objects"
        self.report({'INFO'}, message)
        return {'FINISHED'}


class AEON_PT_OriginAdjuster(Panel):
    """AEON Origin Adjuster Panel"""
    bl_label = "AEON Origin Tools"
    bl_idname = "AEON_PT_origin_adjuster"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AEON Tools"

    def draw(self, context):
        layout = self.layout

        # Header
        row = layout.row()
        row.label(text="Origin Adjuster", icon='OBJECT_ORIGIN')

        # Info box
        box = layout.box()
        box.label(text="Adjust origins for game development:", icon='INFO')
        col = box.column(align=True)
        col.label(text="• Standing: Bottom center origin")
        col.label(text="• Floor: Top center origin")

        layout.separator()

        # Object type detection
        active_obj = context.active_object
        if active_obj and active_obj.type == 'MESH':
            box = layout.box()
            box.label(text=f"Active: {active_obj.name}", icon='OBJECT_DATA')

            # Suggest object type based on name
            obj_name_lower = active_obj.name.lower()
            floor_keywords = ['floor', 'tile', 'platform', 'ground', 'ceiling', 'roof']
            is_likely_floor = any(keyword in obj_name_lower for keyword in floor_keywords)

            if is_likely_floor:
                box.label(text="Detected: Floor object", icon='MESH_PLANE')
            else:
                box.label(text="Detected: Standing object", icon='MESH_CUBE')

        layout.separator()

        # Main buttons
        col = layout.column(align=True)
        col.scale_y = 1.2

        # Standing objects button
        row = col.row(align=True)
        row.operator("aeon.adjust_standing_origin", icon='ANCHOR_BOTTOM')

        # Floor objects button
        row = col.row(align=True)
        row.operator("aeon.adjust_floor_origin", icon='ANCHOR_TOP')

        layout.separator()

        # Batch processing
        box = layout.box()
        box.label(text="Batch Processing:", icon='MODIFIER')
        box.operator("aeon.batch_adjust_origins", icon='AUTO')

        # Usage instructions
        layout.separator()
        box = layout.box()
        box.label(text="Usage:", icon='QUESTION')
        col = box.column(align=True)
        col.label(text="1. Select mesh object(s)")
        col.label(text="2. Choose origin type")
        col.label(text="3. Click adjust button")

        # Selection info
        selected_meshes = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if len(selected_meshes) > 1:
            layout.separator()
            box = layout.box()
            box.label(text=f"{len(selected_meshes)} mesh objects selected", icon='OUTLINER_OB_MESH')


# Registration
classes = (
    AEON_OT_AdjustStandingOrigin,
    AEON_OT_AdjustFloorOrigin,
    AEON_OT_BatchAdjustOrigins,
    AEON_PT_OriginAdjuster,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    print("AEON Origin Adjuster addon registered successfully")

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    print("AEON Origin Adjuster addon unregistered")

if __name__ == "__main__":
    register()