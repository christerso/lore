"""
AEON Interior Toolkit - Enhanced Version
Beautiful tile-based interior creation tools with professional UI dialogs

Author: AEON Engine Development Team
Version: 1.1.0
License: MIT

Features:
- Beautiful dialog-based tile management
- Visual template library with previews
- Professional room creation wizard
- Enhanced tile painter with preview
- Elegant portal and trigger management
"""

bl_info = {
    "name": "AEON Interior Toolkit - Enhanced",
    "author": "AEON Engine Development Team",
    "version": (1, 2, 0),
    "blender": (4, 5, 0),
    "location": "View3D > Sidebar > AEON Tools",
    "description": "Professional tile-based interior creation with beautiful UI - Blender 4.5+ Compatible",
    "category": "Development",
    "warning": "Requires Blender 4.5 or later",
}

import bpy
import bmesh
import mathutils
import math
import json
import os
from mathutils import Vector, Matrix, Quaternion
from bpy.props import (
    BoolProperty, FloatProperty, IntProperty, EnumProperty,
    StringProperty, CollectionProperty, PointerProperty,
    FloatVectorProperty, IntVectorProperty
)
from bpy.types import Panel, Operator, PropertyGroup, UIList

# Blender version compatibility
BLENDER_VERSION = bpy.app.version
BLENDER_4_5_PLUS = BLENDER_VERSION >= (4, 5, 0)

def safe_context_override(context_func):
    """Decorator for safe context override in Blender 4.5+"""
    def wrapper(*args, **kwargs):
        if BLENDER_4_5_PLUS:
            try:
                context_override = bpy.context.copy()
                with bpy.context.temp_override(**context_override):
                    return context_func(*args, **kwargs)
            except:
                # Fallback to direct execution
                return context_func(*args, **kwargs)
        else:
            return context_func(*args, **kwargs)
    return wrapper

# =====================
# ENHANCED PROPERTY GROUPS
# =====================

class AEON_TileTemplateEnhanced(PropertyGroup):
    """Enhanced tile template with preview and metadata"""
    name: StringProperty(name="Tile Name", default="New Tile")
    description: StringProperty(name="Description", default="")
    tile_object: PointerProperty(name="Tile Object", type=bpy.types.Object)
    category: EnumProperty(
        name="Category",
        items=[
            ('FLOOR', 'Floor', 'Floor tiles'),
            ('WALL', 'Wall', 'Wall tiles'),
            ('DOOR', 'Door', 'Door and entrance tiles'),
            ('WINDOW', 'Window', 'Window tiles'),
            ('PROP', 'Prop', 'Furniture and props'),
            ('DECOR', 'Decoration', 'Decorative elements'),
            ('SPECIAL', 'Special', 'Special function tiles'),
        ],
        default='FLOOR'
    )
    preview_scale: FloatProperty(name="Preview Scale", default=1.0)
    tags: StringProperty(name="Tags", default="")  # Comma-separated tags
    author: StringProperty(name="Author", default="")
    date_created: StringProperty(name="Created", default="")
    is_favorite: BoolProperty(name="Favorite", default=False)

class AEON_RoomTemplateEnhanced(PropertyGroup):
    """Enhanced room template with preview and metadata"""
    name: StringProperty(name="Room Name", default="New Room")
    description: StringProperty(name="Description", default="")
    width: IntProperty(name="Width", default=10, min=1, max=100)
    length: IntProperty(name="Length", default=10, min=1, max=100)
    height: FloatProperty(name="Height", default=3.0, min=1.0, max=20.0)
    tile_count: IntProperty(name="Tile Count", default=0)
    room_type: EnumProperty(
        name="Room Type",
        items=[
            ('GENERIC', 'Generic', 'Generic room'),
            ('BEDROOM', 'Bedroom', 'Bedroom'),
            ('KITCHEN', 'Kitchen', 'Kitchen'),
            ('BATHROOM', 'Bathroom', 'Bathroom'),
            ('LIVING', 'Living Room', 'Living room'),
            ('CORRIDOR', 'Corridor', 'Corridor/hallway'),
            ('OFFICE', 'Office', 'Office'),
            ('STORAGE', 'Storage', 'Storage room'),
            ('SPECIAL', 'Special', 'Special purpose room'),
        ],
        default='GENERIC'
    )
    preview_image_path: StringProperty(name="Preview Image", default="")
    author: StringProperty(name="Author", default="")
    date_created: StringProperty(name="Created", default="")
    template_data: StringProperty(name="Template Data", default="")

# =====================
# BEAUTIFUL DIALOG OPERATORS
# =====================

class AEON_OT_TileManagerDialog(Operator):
    """Beautiful tile template management dialog"""
    bl_idname = "aeon.tile_manager_dialog"
    bl_label = "Tile Template Manager"
    bl_description = "Open beautiful tile template manager"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Beautiful header
        box = layout.box()
        row = box.row()
        row.label(text="🎨 Tile Template Manager", icon='MATERIAL')
        row.scale_x = 2.0
        row.label(text="")
        row.operator("aeon.refresh_tile_templates", text="", icon='FILE_REFRESH')
        row.operator("aeon.import_tile_library", text="", icon='IMPORT')
        row.operator("aeon.export_tile_library", text="", icon='EXPORT')

        layout.separator()

        # Category filter
        box = layout.box()
        row = box.row()
        row.label(text="Filter by Category:", icon='FILTER')
        row.prop(scene, "aeon_tile_category_filter", text="")
        row.prop(scene, "aeon_tile_search_filter", text="", icon='VIEWZOOM')

        layout.separator()

        # Template list with beautiful layout
        box = layout.box()
        row = box.row()
        row.label(text="Tile Templates", icon='MESH_CUBE')
        row.operator("aeon.add_tile_template", text="Add New", icon='ADD')
        row.operator("aeon.remove_tile_template", text="Remove", icon='REMOVE')

        # Enhanced template list
        if hasattr(scene, 'aeon_tile_templates_enhanced'):
            filtered_templates = self.get_filtered_templates(scene)

            if filtered_templates:
                for i, template in enumerate(filtered_templates):
                    self.draw_template_item(layout, template, i)
            else:
                row = layout.row()
                row.label(text="No tile templates found", icon='ERROR')
        else:
            row = layout.row()
            row.label(text="Click 'Add New' to create your first tile template", icon='INFO')

    def get_filtered_templates(self, scene):
        """Get templates based on current filters"""
        templates = getattr(scene, 'aeon_tile_templates_enhanced', [])
        category_filter = getattr(scene, 'aeon_tile_category_filter', 'ALL')
        search_filter = getattr(scene, 'aeon_tile_search_filter', '').lower()

        filtered = []
        for template in templates:
            # Category filter
            if category_filter != 'ALL' and template.category != category_filter:
                continue

            # Search filter
            if search_filter:
                if (search_filter not in template.name.lower() and
                    search_filter not in template.description.lower() and
                    search_filter not in template.tags.lower()):
                    continue

            filtered.append(template)

        return filtered

    def draw_template_item(self, layout, template, index):
        """Draw individual template item beautifully"""
        box = layout.box()

        # Main row with preview and info
        row = box.row()

        # Preview area (simulated with icon)
        col = row.column()
        col.scale_x = 0.5
        col.template_icon(icon_value=0, scale=3)
        col.label(text=template.category[0], icon='MESH_CUBE')

        # Template info
        col = row.column()
        col.scale_x = 3.0
        col.label(text=template.name, icon='OBJECT_DATA')
        if template.description:
            col.label(text=template.description, icon='INFO')

        # Category and tags
        col = row.column()
        col.scale_x = 2.0
        col.label(text=template.category, icon='GROUP')
        if template.tags:
            col.label(text=template.tags, icon='MATERIAL')

        # Actions
        col = row.column()
        col.scale_x = 1.0
        if template.is_favorite:
            col.operator("aeon.toggle_tile_favorite", text="", icon='SOLO_ON').index = index
        else:
            col.operator("aeon.toggle_tile_favorite", text="", icon='SOLO_OFF').index = index

        col.operator("aeon.edit_tile_template", text="", icon='EDIT').index = index
        col.operator("aeon.duplicate_tile_template", text="", icon='DUPLICATE').index = index

class AEON_OT_RoomWizardDialog(Operator):
    """Beautiful room creation wizard"""
    bl_idname = "aeon.room_wizard_dialog"
    bl_label = "Room Creation Wizard"
    bl_description = "Create beautiful rooms with step-by-step wizard"
    bl_options = {'REGISTER', 'UNDO'}

    wizard_step: IntProperty(default=1)

    # Step 1: Room Type
    room_type: EnumProperty(
        name="Room Type",
        items=[
            ('EMPTY', 'Empty Room', 'Start with empty room'),
            ('BEDROOM', 'Bedroom', 'Create a bedroom'),
            ('KITCHEN', 'Kitchen', 'Create a kitchen'),
            ('BATHROOM', 'Bathroom', 'Create a bathroom'),
            ('LIVING', 'Living Room', 'Create a living room'),
            ('OFFICE', 'Office', 'Create an office'),
            ('CORRIDOR', 'Corridor', 'Create a corridor'),
            ('CUSTOM', 'Custom', 'Custom room design'),
        ],
        default='EMPTY'
    )

    # Step 2: Dimensions
    room_width: IntProperty(name="Width (tiles)", default=8, min=3, max=30)
    room_length: IntProperty(name="Length (tiles)", default=8, min=3, max=30)
    room_height: FloatProperty(name="Height (meters)", default=3.0, min=2.0, max=10.0)

    # Step 3: Style
    room_style: EnumProperty(
        name="Style",
        items=[
            ('MODERN', 'Modern', 'Modern style'),
            ('CLASSIC', 'Classic', 'Classic style'),
            ('MINIMAL', 'Minimal', 'Minimal style'),
            ('INDUSTRIAL', 'Industrial', 'Industrial style'),
            ('FUTURISTIC', 'Futuristic', 'Futuristic style'),
        ],
        default='MODERN'
    )

    # Step 4: Features
    include_doors: BoolProperty(name="Include Doors", default=True)
    include_windows: BoolProperty(name="Include Windows", default=True)
    include_lighting: BoolProperty(name="Include Lighting", default=True)
    include_furniture: BoolProperty(name="Include Furniture", default=False)

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        self.wizard_step = 1
        return wm.invoke_props_dialog(self, width=500)

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Beautiful wizard header
        box = layout.box()
        row = box.row()
        row.label(text=f"🏠 Room Creation Wizard - Step {self.wizard_step}/4", icon='HOME')

        # Progress bar
        row = box.row()
        for i in range(1, 5):
            if i <= self.wizard_step:
                row.operator("aeon.wizard_step", text=str(i), depress=True).step = i
            else:
                row.operator("aeon.wizard_step", text=str(i)).step = i

        layout.separator()

        # Step content
        if self.wizard_step == 1:
            self.draw_step_room_type(layout)
        elif self.wizard_step == 2:
            self.draw_step_dimensions(layout)
        elif self.wizard_step == 3:
            self.draw_step_style(layout)
        elif self.wizard_step == 4:
            self.draw_step_features(layout)

        # Navigation buttons
        layout.separator()
        box = layout.box()
        row = box.row()

        if self.wizard_step > 1:
            row.operator("aeon.wizard_previous", text="← Previous", icon='BACK')

        row.scale_x = 2.0
        row.label(text="")

        if self.wizard_step < 4:
            row.operator("aeon.wizard_next", text="Next →", icon='FORWARD')
        else:
            row.operator("aeon.wizard_create_room", text="Create Room", icon='CHECKMARK')

    def draw_step_room_type(self, layout):
        box = layout.box()
        box.label(text="What type of room would you like to create?", icon='QUESTION')
        box.prop(self, "room_type")

    def draw_step_dimensions(self, layout):
        box = layout.box()
        box.label(text="Set the room dimensions:", icon='ARROW_LEFTRIGHT')
        col = box.column(align=True)
        col.prop(self, "room_width")
        col.prop(self, "room_length")
        col.prop(self, "room_height")

    def draw_step_style(self, layout):
        box = layout.box()
        box.label(text="Choose the room style:", icon='MATERIAL')
        box.prop(self, "room_style")

    def draw_step_features(self, layout):
        box = layout.box()
        box.label(text="Select additional features:", icon='MODIFIER')
        col = box.column(align=True)
        col.prop(self, "include_doors")
        col.prop(self, "include_windows")
        col.prop(self, "include_lighting")
        col.prop(self, "include_furniture")

class AEON_OT_PortalManagerDialog(Operator):
    """Beautiful portal management dialog"""
    bl_idname = "aeon.portal_manager_dialog"
    bl_label = "Portal Manager"
    bl_description = "Manage level portals and transitions"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=700)

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Beautiful header
        box = layout.box()
        row = box.row()
        row.label(text="🚪 Portal Manager", icon='WORLD')
        row.scale_x = 2.0
        row.label(text="")
        row.operator("aeon.test_portal_connections", text="Test Connections", icon='PLAY')
        row.operator("aeon.portal_visualizer", text="Visualize", icon='HIDE_OFF')

        layout.separator()

        # Portal list
        box = layout.box()
        row = box.row()
        row.label(text="Active Portals", icon='FORWARD')
        row.operator("aeon.add_portal", text="Add Portal", icon='ADD')

        if hasattr(scene, 'aeon_portals_enhanced') and scene.aeon_portals_enhanced:
            for portal in scene.aeon_portals_enhanced:
                self.draw_portal_item(layout, portal)
        else:
            row = layout.row()
            row.label(text="No portals defined", icon='INFO')

    def draw_portal_item(self, layout, portal):
        """Draw portal item beautifully"""
        box = layout.box()
        row = box.row()

        # Portal info
        col = row.column()
        col.scale_x = 3.0
        col.label(text=portal.name, icon='OBJECT_DATA')
        col.label(text=f"→ {portal.target_map}", icon='WORLD')

        # Portal type and status
        col = row.column()
        col.scale_x = 2.0
        col.label(text=portal.portal_type, icon='MESH_CUBE')
        status_icon = 'CHECKMARK' if portal.is_active else 'X'
        col.label(text="Active" if portal.is_active else "Inactive", icon=status_icon)

        # Actions
        col = row.column()
        col.scale_x = 1.0
        col.operator("aeon.edit_portal", text="", icon='EDIT').portal_name = portal.name
        col.operator("aeon.remove_portal", text="", icon='REMOVE').portal_name = portal.name

class AEON_OT_TriggerManagerDialog(Operator):
    """Beautiful trigger management dialog"""
    bl_idname = "aeon.trigger_manager_dialog"
    bl_label = "Trigger Manager"
    bl_description = "Manage game triggers and events"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=800)

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Beautiful header
        box = layout.box()
        row = box.row()
        row.label(text="⚡ Trigger Manager", icon='HAND')
        row.scale_x = 2.0
        row.label(text="")
        row.operator("aeon.trigger_test_mode", text="Test Mode", icon='PLAY')
        row.operator("aeon.trigger_visualizer", text="Visualize", icon='HIDE_OFF')

        layout.separator()

        # Trigger list
        box = layout.box()
        row = box.row()
        row.label(text="Game Triggers", icon='DECORATE_DRIVER')
        row.operator("aeon.add_trigger", text="Add Trigger", icon='ADD')

        if hasattr(scene, 'aeon_triggers_enhanced') and scene.aeon_triggers_enhanced:
            for trigger in scene.aeon_triggers_enhanced:
                self.draw_trigger_item(layout, trigger)
        else:
            row = layout.row()
            row.label(text="No triggers defined", icon='INFO')

    def draw_trigger_item(self, layout, trigger):
        """Draw trigger item beautifully"""
        box = layout.box()
        row = box.row()

        # Trigger info
        col = row.column()
        col.scale_x = 3.0
        col.label(text=trigger.name, icon='OBJECT_DATA')
        col.label(text=f"Event: {trigger.event_name}", icon='ACTION')

        # Trigger type and target
        col = row.column()
        col.scale_x = 2.0
        col.label(text=trigger.trigger_type, icon='HAND')
        if trigger.target_object:
            col.label(text=f"Target: {trigger.target_object}", icon='CONSTRAINT')

        # Status and actions
        col = row.column()
        col.scale_x = 1.0
        status_icon = 'CHECKMARK' if trigger.is_active else 'X'
        col.operator("aeon.toggle_trigger", text="", icon=status_icon).trigger_name = trigger.name
        col.operator("aeon.edit_trigger", text="", icon='EDIT').trigger_name = trigger.name
        col.operator("aeon.remove_trigger", text="", icon='REMOVE').trigger_name = trigger.name

# =====================
# ENHANCED TILE PAINTER
# =====================

class AEON_OT_EnhancedTilePainter(Operator):
    """Enhanced tile painter with beautiful UI and feedback"""
    bl_idname = "aeon.enhanced_tile_painter"
    bl_label = "Enhanced Tile Painter"
    bl_description = "Professional tile painting with visual feedback"
    bl_options = {'REGISTER', 'UNDO'}

    paint_mode: EnumProperty(
        name="Paint Mode",
        items=[
            ('PLACE', 'Place', 'Place single tiles'),
            ('PAINT', 'Paint', 'Continuous painting'),
            ('REMOVE', 'Remove', 'Remove tiles'),
            ('FILL', 'Fill Area', 'Fill connected area'),
            ('LINE', 'Line', 'Paint in straight line'),
            ('RECTANGLE', 'Rectangle', 'Paint rectangle area'),
        ],
        default='PLACE'
    )

    grid_size: FloatProperty(name="Grid Size", default=1.0, min=0.1, max=10.0)
    paint_height: FloatProperty(name="Height", default=0.0, min=-100.0, max=100.0)

    # Visual feedback
    show_preview: BoolProperty(name="Show Preview", default=True)
    preview_color: FloatVectorProperty(
        name="Preview Color",
        default=(0.0, 1.0, 0.0, 0.5),
        subtype='COLOR',
        size=4
    )

    @classmethod
    def poll(cls, context):
        return context.space_data.type == 'VIEW_3D'

    def invoke(self, context, event):
        # Check if we have tile templates
        if not hasattr(context.scene, 'aeon_tile_templates_enhanced') or not context.scene.aeon_tile_templates_enhanced:
            self.report({'WARNING'}, "Please create tile templates first!")
            return {'CANCELLED'}

        # Get selected tile template
        selected_index = getattr(context.scene, 'aeon_selected_tile_template_index', 0)
        templates = getattr(context.scene, 'aeon_tile_templates_enhanced', [])

        if selected_index >= len(templates) or not templates[selected_index].tile_object:
            self.report({'WARNING'}, "Please select a valid tile template!")
            return {'CANCELLED'}

        # Store initial state
        self.initial_selection = context.selected_objects.copy()
        self.initial_mode = context.mode
        self.selected_template = templates[selected_index]

        # Enable modal mode
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type == 'ESC':
            return self.cancel(context)

        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                return self.start_painting(context, event)
            elif event.value == 'RELEASE':
                return self.stop_painting(context, event)

        if event.type == 'RIGHTMOUSE' and event.value == 'PRESS':
            return self.cancel(context)

        return {'PASS_THROUGH'}

    def start_painting(self, context, event):
        """Start painting operation"""
        self.is_painting = True
        return self.paint_tile(context, event)

    def stop_painting(self, context, event):
        """Stop painting operation"""
        self.is_painting = False
        return {'RUNNING_MODAL'}

    def paint_tile(self, context, event):
        """Paint tile at mouse position"""
        # Get mouse position in 3D space
        region = context.region
        rv3d = context.region_data

        # Get ray from mouse position
        view_vector = rv3d.view_rotation @ Vector((0, 0, -1))
        ray_origin = rv3d.view_location

        # Cast ray to grid plane
        if view_vector.z != 0:
            t = (self.paint_height - ray_origin.z) / view_vector.z
            if t > 0:
                hit_point = ray_origin + view_vector * t

                # Snap to grid
                grid_pos = Vector((
                    round(hit_point.x / self.grid_size) * self.grid_size,
                    round(hit_point.y / self.grid_size) * self.grid_size,
                    self.paint_height
                ))

                # Place tile
                if self.paint_mode in ['PLACE', 'PAINT']:
                    self.place_tile_at_grid(context, grid_pos)
                elif self.paint_mode == 'REMOVE':
                    self.remove_tile_at_grid(context, grid_pos)

        if self.paint_mode == 'PAINT' and self.is_painting:
            return {'RUNNING_MODAL'}
        else:
            return {'PASS_THROUGH'}

    def place_tile_at_grid(self, context, position):
        """Place tile at grid position"""
        if not self.selected_template or not self.selected_template.tile_object:
            return

        # Check if tile already exists at position
        existing_tile = self.find_tile_at_position(context, position)
        if existing_tile:
            return  # Don't place duplicate tiles

        # Duplicate tile template
        bpy.ops.object.select_all(action='DESELECT')
        template_obj = self.selected_template.tile_object
        template_obj.select_set(True)
        context.view_layer.objects.active = template_obj

        bpy.ops.object.duplicate()
        new_tile = context.active_object
        new_tile.matrix_world.translation = position
        new_tile.name = f"{self.selected_template.name}_{int(position.x)}_{int(position.y)}"

        # Add to painted tiles group
        self.add_to_tile_group(new_tile)

    def remove_tile_at_grid(self, context, position):
        """Remove tile at grid position"""
        tile = self.find_tile_at_position(context, position)
        if tile:
            bpy.data.objects.remove(tile, do_unlink=True)

    def find_tile_at_position(self, context, position):
        """Find tile at specific position"""
        threshold = self.grid_size * 0.5

        for obj in bpy.data.objects:
            if obj.type == 'MESH':
                distance = (obj.matrix_world.translation - position).length
                if distance <= threshold:
                    # Check if object has tile name pattern
                    if any(tile_type in obj.name.lower() for tile_type in ['floor', 'wall', 'tile']):
                        return obj
        return None

    def add_to_tile_group(self, tile_obj):
        """Add tile to painted tiles group"""
        group_name = "Painted_Tiles"
        if group_name not in bpy.data.collections:
            tile_group = bpy.data.collections.new(group_name)
            context.scene.collection.children.link(tile_group)
        else:
            tile_group = bpy.data.collections[group_name]

        tile_group.objects.link(tile_obj)
        context.scene.collection.objects.unlink(tile_obj)

    def cancel(self, context):
        # Restore initial state
        return {'CANCELLED'}

# =====================
# MAIN ENHANCED PANEL
# =====================

class AEON_PT_InteriorToolkitEnhanced(Panel):
    """Enhanced AEON Interior Toolkit Panel"""
    bl_label = "Interior Toolkit Pro"
    bl_idname = "AEON_PT_interior_toolkit_enhanced"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AEON Tools"

    def draw(self, context):
        layout = self.layout

        # Beautiful header
        box = layout.box()
        row = box.row()
        row.label(text="🏠 AEON Interior Toolkit Pro", icon='HOME')
        row.scale_x = 2.0
        row.label(text="")
        row.operator("aeon.about_toolkit", text="", icon='INFO')

        layout.separator()

        # Quick Actions
        box = layout.box()
        box.label(text="Quick Actions", icon='SCRIPT')
        col = box.column(align=True)

        # Tile painter
        row = col.row()
        row.operator("aeon.enhanced_tile_painter", text="🎨 Tile Painter", icon='BRUSH_PAINT')
        row.operator("aeon.tile_manager_dialog", text="", icon='SETTINGS')

        # Room creation
        row = col.row()
        row.operator("aeon.room_wizard_dialog", text="🏠 Room Wizard", icon='MESH_GRID')
        row.operator("aeon.quick_room", text="Quick", icon='MESH_CUBE')

        # Portal and trigger managers
        row = col.row()
        row.operator("aeon.portal_manager_dialog", text="🚪 Portals", icon='WORLD')
        row.operator("aeon.trigger_manager_dialog", text="⚡ Triggers", icon='HAND')

        layout.separator()

        # Quick Views
        box = layout.box()
        box.label(text="Quick Views", icon='VIEW_CAMERA')
        row = box.row()
        row.operator("aeon.quick_view", text="Top").view_mode = 'TOP'
        row.operator("aeon.quick_view", text="Front").view_mode = 'FRONT'
        row.operator("aeon.quick_view", text="Side").view_mode = 'SIDE'
        row = box.row()
        row.operator("aeon.quick_view", text="ISO").view_mode = 'ISO'
        row.operator("aeon.quick_view", text="Tile").view_mode = 'TILE_TOP'
        row.operator("aeon.quick_view", text="Persp").view_mode = 'PERSP'

        layout.separator()

        # Tools Summary
        box = layout.box()
        box.label(text="Tools Summary", icon='SUMMARY')
        scene = context.scene

        col = box.column(align=True)

        # Tile templates count
        tile_count = len(getattr(scene, 'aeon_tile_templates_enhanced', []))
        col.label(text=f"🎨 Tile Templates: {tile_count}")

        # Room templates count
        room_count = len(getattr(scene, 'aeon_room_templates_enhanced', []))
        col.label(text=f"🏠 Room Templates: {room_count}")

        # Portals count
        portal_count = len(getattr(scene, 'aeon_portals_enhanced', []))
        col.label(text=f"🚪 Portals: {portal_count}")

        # Triggers count
        trigger_count = len(getattr(scene, 'aeon_triggers_enhanced', []))
        col.label(text=f"⚡ Triggers: {trigger_count}")

        layout.separator()

        # Export
        box = layout.box()
        box.label(text="Export Level Data", icon='EXPORT')
        col = box.column(align=True)
        col.operator("aeon.export_level_data_enhanced", text="📤 Export Level Data")
        col.operator("aeon.export_level_report", text="📋 Export Report")

# =====================
# UTILITY OPERATORS
# =====================

class AEON_OT_AboutToolkit(Operator):
    """Show about dialog"""
    bl_idname = "aeon.about_toolkit"
    bl_label = "About AEON Toolkit"
    bl_description = "Show information about the toolkit"
    bl_options = {'REGISTER'}

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        row = box.row()
        row.label(text="🏠 AEON Interior Toolkit Pro", icon='HOME')

        box = layout.box()
        box.label(text="Version: 1.1.0", icon='INFO')
        box.label(text="Author: AEON Engine Development Team", icon='USER')
        box.label(text="License: MIT", icon='FILE_BLEND')

        box = layout.box()
        box.label(text="Professional tile-based interior creation tools", icon='MESH_DATA')
        box.label(text="for C++/Vulkan/DirectX game development", icon='SHADERFX')

class AEON_OT_QuickRoom(Operator):
    """Quick room creation with defaults"""
    bl_idname = "aeon.quick_room"
    bl_label = "Quick Room"
    bl_description = "Create a quick room with default settings"
    bl_options = {'REGISTER', 'UNDO'}

    room_size: EnumProperty(
        name="Room Size",
        items=[
            ('SMALL', 'Small (6x6)', 'Small 6x6 room'),
            ('MEDIUM', 'Medium (10x10)', 'Medium 10x10 room'),
            ('LARGE', 'Large (15x15)', 'Large 15x15 room'),
            ('HUGE', 'Huge (20x20)', 'Huge 20x20 room'),
        ],
        default='MEDIUM'
    )

    def execute(self, context):
        size_map = {
            'SMALL': 6,
            'MEDIUM': 10,
            'LARGE': 15,
            'HUGE': 20
        }

        size = size_map[self.room_size]
        self.create_quick_room(context, size, size)

        self.report({'INFO'}, f"Created {self.room_size.lower()} room ({size}x{size})")
        return {'FINISHED'}

    def create_quick_room(self, context, width, length):
        """Create a quick room with floor and walls"""
        # Create floor
        bpy.ops.mesh.primitive_plane_add(
            size=1,
            location=(0, 0, 0)
        )
        floor = context.active_object
        floor.name = "Quick_Room_Floor"
        floor.scale.x = width / 2
        floor.scale.y = length / 2

        # Create walls
        wall_height = 3.0
        wall_thickness = 0.1

        # North wall
        bpy.ops.mesh.primitive_cube_add(
            location=(0, length/2, wall_height/2),
            scale=(width/2, wall_thickness, wall_height/2)
        )
        context.active_object.name = "Quick_Room_Wall_North"

        # South wall
        bpy.ops.mesh.primitive_cube_add(
            location=(0, -length/2, wall_height/2),
            scale=(width/2, wall_thickness, wall_height/2)
        )
        context.active_object.name = "Quick_Room_Wall_South"

        # East wall
        bpy.ops.mesh.primitive_cube_add(
            location=(width/2, 0, wall_height/2),
            scale=(wall_thickness, length/2, wall_height/2)
        )
        context.active_object.name = "Quick_Room_Wall_East"

        # West wall
        bpy.ops.mesh.primitive_cube_add(
            location=(-width/2, 0, wall_height/2),
            scale=(wall_thickness, length/2, wall_height/2)
        )
        context.active_object.name = "Quick_Room_Wall_West"

# =====================
# REGISTRATION
# =====================

def register():
    # Register property groups
    bpy.utils.register_class(AEON_TileTemplateEnhanced)
    bpy.utils.register_class(AEON_RoomTemplateEnhanced)

    # Register dialog operators
    bpy.utils.register_class(AEON_OT_TileManagerDialog)
    bpy.utils.register_class(AEON_OT_RoomWizardDialog)
    bpy.utils.register_class(AEON_OT_PortalManagerDialog)
    bpy.utils.register_class(AEON_OT_TriggerManagerDialog)

    # Register main operators
    bpy.utils.register_class(AEON_OT_EnhancedTilePainter)
    bpy.utils.register_class(AEON_OT_AboutToolkit)
    bpy.utils.register_class(AEON_OT_QuickRoom)

    # Register panel
    bpy.utils.register_class(AEON_PT_InteriorToolkitEnhanced)

    # Add scene properties
    bpy.types.Scene.aeon_tile_templates_enhanced = bpy.props.CollectionProperty(type=AEON_TileTemplateEnhanced)
    bpy.types.Scene.aeon_selected_tile_template_index = bpy.props.IntProperty(default=0)
    bpy.types.Scene.aeon_tile_category_filter = bpy.props.EnumProperty(
        name="Category Filter",
        items=[
            ('ALL', 'All', 'All categories'),
            ('FLOOR', 'Floor', 'Floor tiles'),
            ('WALL', 'Wall', 'Wall tiles'),
            ('DOOR', 'Door', 'Door tiles'),
            ('WINDOW', 'Window', 'Window tiles'),
            ('PROP', 'Props', 'Furniture and props'),
            ('DECOR', 'Decoration', 'Decorative elements'),
            ('SPECIAL', 'Special', 'Special function tiles'),
        ],
        default='ALL'
    )
    bpy.types.Scene.aeon_tile_search_filter = bpy.props.StringProperty(name="Search Filter", default="")

    bpy.types.Scene.aeon_room_templates_enhanced = bpy.props.CollectionProperty(type=AEON_RoomTemplateEnhanced)
    bpy.types.Scene.aeon_selected_room_template_index = bpy.props.IntProperty(default=0)

    bpy.types.Scene.aeon_portals_enhanced = bpy.props.CollectionProperty(type=AEON_Portal)
    bpy.types.Scene.aeon_triggers_enhanced = bpy.props.CollectionProperty(type=AEON_Trigger)

    print("AEON Interior Toolkit Pro registered successfully")

def unregister():
    # Unregister panel
    bpy.utils.unregister_class(AEON_PT_InteriorToolkitEnhanced)

    # Unregister main operators
    bpy.utils.unregister_class(AEON_OT_QuickRoom)
    bpy.utils.unregister_class(AEON_OT_AboutToolkit)
    bpy.utils.unregister_class(AEON_OT_EnhancedTilePainter)

    # Unregister dialog operators
    bpy.utils.unregister_class(AEON_OT_TriggerManagerDialog)
    bpy.utils.unregister_class(AEON_OT_PortalManagerDialog)
    bpy.utils.unregister_class(AEON_OT_RoomWizardDialog)
    bpy.utils.unregister_class(AEON_OT_TileManagerDialog)

    # Unregister property groups
    bpy.utils.unregister_class(AEON_RoomTemplateEnhanced)
    bpy.utils.unregister_class(AEON_TileTemplateEnhanced)

    # Remove scene properties
    del bpy.types.Scene.aeon_triggers_enhanced
    del bpy.types.Scene.aeon_portals_enhanced
    del bpy.types.Scene.aeon_selected_room_template_index
    del bpy.types.Scene.aeon_room_templates_enhanced
    del bpy.types.Scene.aeon_selected_tile_template_index
    del bpy.types.Scene.aeon_tile_search_filter
    del bpy.types.Scene.aeon_tile_category_filter
    del bpy.types.Scene.aeon_tile_templates_enhanced

    print("AEON Interior Toolkit Pro unregistered")

if __name__ == "__main__":
    register()