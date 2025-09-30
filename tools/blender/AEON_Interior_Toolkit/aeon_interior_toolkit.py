"""
AEON Interior Toolkit
Specialized tile-based interior creation tools for game development

Author: AEON Engine Development Team
Version: 1.0.0
License: MIT

Features:
- Template-based floor layout system
- Visual tile painter with preview grid
- Portal/transition system for level loading
- Quick view switching for level design
- Trigger/event metadata system for game logic
"""

bl_info = {
    "name": "AEON Interior Toolkit",
    "author": "AEON Engine Development Team",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > AEON Tools",
    "description": "Professional tile-based interior creation tools",
    "category": "Development",
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

# =====================
# TEMPLATE SYSTEM
# =====================

class AEON_RoomTemplate(PropertyGroup):
    """Room template property group"""
    name: StringProperty(name="Template Name", default="RoomTemplate")
    description: StringProperty(name="Description", default="")
    width: IntProperty(name="Width", default=10, min=1, max=100)
    length: IntProperty(name="Length", default=10, min=1, max=100)
    template_data: StringProperty(name="Template Data", default="")

class AEON_TileTemplate(PropertyGroup):
    """Tile template property group"""
    name: StringProperty(name="Tile Name", default="Tile")
    description: StringProperty(name="Description", default="")
    tile_object: PointerProperty(name="Tile Object", type=bpy.types.Object)
    category: StringProperty(name="Category", default="Floor")
    preview_scale: FloatProperty(name="Preview Scale", default=1.0)

class AEON_OT_CreateRoomTemplate(Operator):
    """Create room template from current selection"""
    bl_idname = "aeon.create_room_template"
    bl_label = "Create Room Template"
    bl_description = "Save current room layout as template"
    bl_options = {'REGISTER', 'UNDO'}

    template_name: StringProperty(
        name="Template Name",
        description="Name for the room template",
        default="New_Room_Template"
    )

    include_objects: BoolProperty(
        name="Include Objects",
        description="Include furniture and props in template",
        default=True
    )

    @classmethod
    def poll(cls, context):
        return len(context.selected_objects) > 0

    def execute(self, context):
        # Get all selected objects
        selected_objects = context.selected_objects.copy()

        if not selected_objects:
            self.report({'WARNING'}, "No objects selected")
            return {'CANCELLED'}

        # Calculate room bounds
        min_bounds = Vector((float('inf'), float('inf'), float('inf')))
        max_bounds = Vector((float('-inf'), float('-inf'), float('-inf')))

        for obj in selected_objects:
            for corner in obj.bound_box:
                world_corner = obj.matrix_world @ Vector(corner)
                min_bounds = Vector(min(min_bounds[i], world_corner[i]) for i in range(3))
                max_bounds = Vector(max(max_bounds[i], world_corner[i]) for i in range(3))

        # Calculate dimensions in tiles (assume 1 unit = 1 tile)
        width = int(math.ceil(max_bounds.x - min_bounds.x))
        length = int(math.ceil(max_bounds.y - min_bounds.y))

        # Create template data
        template_data = {
            'name': self.template_name,
            'width': width,
            'length': length,
            'bounds': {
                'min': list(min_bounds),
                'max': list(max_bounds)
            },
            'objects': []
        }

        if self.include_objects:
            for obj in selected_objects:
                obj_data = {
                    'name': obj.name,
                    'type': obj.type,
                    'location': list(obj.matrix_world.translation),
                    'rotation': list(obj.matrix_world.to_euler()),
                    'scale': list(obj.scale),
                    'data_path': obj.data.name if obj.data else None
                }
                template_data['objects'].append(obj_data)

        # Add template to scene properties
        if not hasattr(context.scene, 'aeon_room_templates'):
            context.scene.aeon_room_templates = []

        template = context.scene.aeon_room_templates.add()
        template.name = self.template_name
        template.width = width
        template.length = length
        template.template_data = json.dumps(template_data)

        self.report({'INFO'}, f"Created room template: {self.template_name} ({width}x{length})")
        return {'FINISHED'}

# =====================
# TILE PAINTER SYSTEM
# =====================

class AEON_OT_TilePainter(Operator):
    """Paint tiles with visual preview system"""
    bl_idname = "aeon.tile_painter"
    bl_label = "Tile Painter"
    bl_description = "Paint tiles with visual feedback"
    bl_options = {'REGISTER', 'UNDO'}

    paint_mode: EnumProperty(
        name="Paint Mode",
        description="Tile painting mode",
        items=[
            ('PLACE', 'Place', 'Place tiles'),
            ('REMOVE', 'Remove', 'Remove tiles'),
            ('PAINT', 'Paint', 'Continuous paint'),
            ('FILL', 'Fill', 'Fill area'),
        ],
        default='PLACE'
    )

    tile_category: EnumProperty(
        name="Category",
        description="Tile category to filter",
        items=[
            ('ALL', 'All', 'Show all tiles'),
            ('FLOOR', 'Floor', 'Floor tiles'),
            ('WALL', 'Wall', 'Wall tiles'),
            ('PROP', 'Props', 'Prop objects'),
            ('SPECIAL', 'Special', 'Special tiles'),
        ],
        default='ALL'
    )

    grid_size: FloatProperty(
        name="Grid Size",
        description="Tile grid size",
        default=1.0,
        min=0.1,
        max=10.0
    )

    paint_height: FloatProperty(
        name="Paint Height",
        description="Height for tile placement",
        default=0.0,
        min=-100.0,
        max=100.0
    )

    @classmethod
    def poll(cls, context):
        return context.space_data.type == 'VIEW_3D'

    def invoke(self, context, event):
        # Store initial state
        self.initial_selection = context.selected_objects.copy()
        self.initial_mode = context.mode

        # Enable paint mode
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type == 'ESC':
            return self.cancel(context)

        if event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            return self.paint_tile(context, event)

        if event.type == 'RIGHTMOUSE' and event.value == 'PRESS':
            return self.cancel(context)

        return {'PASS_THROUGH'}

    def paint_tile(self, context, event):
        """Paint tile at mouse position"""
        # Get mouse position in 3D space
        region = context.region
        rv3d = context.region_data

        # Get mouse coordinates
        mouse_x = event.mouse_region_x
        mouse_y = event.mouse_region_y

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

                # Place or remove tile based on mode
                if self.paint_mode in ['PLACE', 'PAINT']:
                    self.place_tile_at_grid(context, grid_pos)
                elif self.paint_mode == 'REMOVE':
                    self.remove_tile_at_grid(context, grid_pos)
                elif self.paint_mode == 'FILL':
                    self.fill_area(context, grid_pos)

        return {'RUNNING_MODAL'}

    def place_tile_at_grid(self, context, position):
        """Place tile at grid position"""
        # Get selected tile template
        selected_tile = getattr(context.scene, 'aeon_selected_tile_template', None)
        if not selected_tile:
            self.report({'WARNING'}, "No tile template selected")
            return

        tile_obj = selected_tile.tile_object
        if not tile_obj:
            return

        # Duplicate tile object
        bpy.ops.object.select_all(action='DESELECT')
        tile_obj.select_set(True)
        context.view_layer.objects.active = tile_obj

        bpy.ops.object.duplicate()
        new_tile = context.active_object
        new_tile.matrix_world.translation = position

        # Add to tile group
        tile_group_name = "Painted_Tiles"
        if tile_group_name not in bpy.data.collections:
            tile_group = bpy.data.collections.new(tile_group_name)
            context.scene.collection.children.link(tile_group)
        else:
            tile_group = bpy.data.collections[tile_group_name]

        tile_group.objects.link(new_tile)
        context.scene.collection.objects.unlink(new_tile)

    def remove_tile_at_grid(self, context, position):
        """Remove tile at grid position"""
        # Find tile at position
        threshold = self.grid_size * 0.5
        tiles_to_remove = []

        for obj in bpy.data.objects:
            if obj.type == 'MESH':
                distance = (obj.matrix_world.translation - position).length
                if distance <= threshold:
                    tiles_to_remove.append(obj)

        # Remove tiles
        for obj in tiles_to_remove:
            bpy.data.objects.remove(obj, do_unlink=True)

    def fill_area(self, context, start_position):
        """Fill connected area with tiles"""
        # Simplified flood fill implementation
        # This would scan for connected tiles and fill the area
        self.place_tile_at_grid(context, start_position)

    def cancel(self, context):
        # Restore initial state
        return {'CANCELLED'}

# =====================
# PORTAL SYSTEM
# =====================

class AEON_Portal(PropertyGroup):
    """Portal property group"""
    name: StringProperty(name="Portal Name", default="Portal")
    target_map: StringProperty(name="Target Map", default="")
    target_portal: StringProperty(name="Target Portal", default="")
    portal_type: EnumProperty(
        name="Portal Type",
        items=[
            ('DOOR', 'Door', 'Door portal'),
            ('TELEPORTER', 'Teleporter', 'Teleporter portal'),
            ('LEVEL_TRANSITION', 'Level Transition', 'Level transition portal'),
            ('SECRET', 'Secret', 'Secret portal'),
        ],
        default='DOOR'
    )
    is_bidirectional: BoolProperty(name="Bidirectional", default=True)
    transition_effect: StringProperty(name="Transition Effect", default="fade")
    load_trigger: EnumProperty(
        name="Load Trigger",
        items=[
            ('ON_TOUCH', 'On Touch', 'Load when player touches'),
            ('ON_USE', 'On Use', 'Load when player uses'),
            ('AUTOMATIC', 'Automatic', 'Load automatically'),
        ],
        default='ON_TOUCH'
    )

class AEON_OT_MarkPortal(Operator):
    """Mark selected object as portal"""
    bl_idname = "aeon.mark_portal"
    bl_label = "Mark as Portal"
    bl_description = "Mark selected object as level transition portal"
    bl_options = {'REGISTER', 'UNDO'}

    portal_name: StringProperty(
        name="Portal Name",
        description="Name of the portal",
        default="Portal_001"
    )

    target_map: StringProperty(
        name="Target Map",
        description="Target level/map file",
        default="next_level.map"
    )

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH'

    def execute(self, context):
        obj = context.active_object

        # Add portal properties to object
        if not hasattr(obj, 'aeon_portal_data'):
            obj['aeon_portal_data'] = {}

        portal_data = {
            'name': self.portal_name,
            'target_map': self.target_map,
            'type': 'level_transition',
            'is_bidirectional': True,
            'load_trigger': 'on_touch'
        }

        obj['aeon_portal_data'] = portal_data

        # Add portal marker visual
        self.add_portal_marker(obj)

        # Add to scene portals list
        if not hasattr(context.scene, 'aeon_portals'):
            context.scene.aeon_portals = []

        portal = context.scene.aeon_portals.add()
        portal.name = self.portal_name
        portal.target_map = self.target_map

        self.report({'INFO'}, f"Marked {obj.name} as portal to {self.target_map}")
        return {'FINISHED'}

    def add_portal_marker(self, obj):
        """Add visual marker for portal"""
        # Add empty or special material to indicate portal
        if not obj.data.materials:
            mat = bpy.data.materials.new(name="Portal_Material")
            mat.use_nodes = True
            obj.data.materials.append(mat)

# =====================
# QUICK VIEW SYSTEM
# =====================

class AEON_OT_QuickView(Operator):
    """Quick view switching for level design"""
    bl_idname = "aeon.quick_view"
    bl_label = "Quick View"
    bl_description = "Switch to predefined view presets"
    bl_options = {'REGISTER'}

    view_mode: EnumProperty(
        name="View Mode",
        description="Quick view preset",
        items=[
            ('TOP', 'Top Ortho', 'Top orthographic view'),
            ('FRONT', 'Front Ortho', 'Front orthographic view'),
            ('SIDE', 'Side Ortho', 'Side orthographic view'),
            ('ISO', 'Isometric', 'Isometric view'),
            ('PERSP', 'Perspective', 'Perspective view'),
            ('TILE_TOP', 'Tile Top', 'Top view for tile placement'),
        ],
        default='TILE_TOP'
    )

    @classmethod
    def poll(cls, context):
        return context.space_data.type == 'VIEW_3D'

    def execute(self, context):
        region = context.region
        rv3d = context.region_data

        if self.view_mode == 'TOP':
            rv3d.view_rotation = Quaternion((1, 0, 0, 0))
            rv3d.view_perspective = 'ORTHO'
        elif self.view_mode == 'FRONT':
            rv3d.view_rotation = Quaternion((0.707, 0.707, 0, 0))
            rv3d.view_perspective = 'ORTHO'
        elif self.view_mode == 'SIDE':
            rv3d.view_rotation = Quaternion((0.5, 0.5, 0.5, 0.5))
            rv3d.view_perspective = 'ORTHO'
        elif self.view_mode == 'ISO':
            rv3d.view_rotation = Quaternion((0.354, 0.146, 0.354, 0.854))
            rv3d.view_perspective = 'ORTHO'
        elif self.view_mode == 'PERSP':
            rv3d.view_perspective = 'PERSPECTIVE'
        elif self.view_mode == 'TILE_TOP':
            rv3d.view_rotation = Quaternion((1, 0, 0, 0))
            rv3d.view_perspective = 'ORTHO'
            # Zoom out for better tile placement view
            rv3d.view_distance = 20

        # Update view
        context.region.tag_redraw()

        return {'FINISHED'}

# =====================
# TRIGGER/EVENT SYSTEM
# =====================

class AEON_Trigger(PropertyGroup):
    """Trigger property group"""
    name: StringProperty(name="Trigger Name", default="Trigger")
    trigger_type: EnumProperty(
        name="Trigger Type",
        items=[
            ('ON_ENTER', 'On Enter', 'Trigger when object enters'),
            ('ON_EXIT', 'On Exit', 'Trigger when object exits'),
            ('ON_USE', 'On Use', 'Trigger when player uses'),
            ('ON_TOUCH', 'On Touch', 'Trigger when touched'),
            ('ON_PROXIMITY', 'On Proximity', 'Trigger when near'),
            ('ON_TIMER', 'On Timer', 'Trigger after time delay'),
        ],
        default='ON_ENTER'
    )
    event_name: StringProperty(name="Event Name", default="event_trigger")
    target_object: StringProperty(name="Target Object", default="")
    parameters: StringProperty(name="Parameters", default="{}")
    is_active: BoolProperty(name="Active", default=True)
    cooldown: FloatProperty(name="Cooldown", default=0.0)

class AEON_OT_AttachTrigger(Operator):
    """Attach trigger/event metadata to object"""
    bl_idname = "aeon.attach_trigger"
    bl_label = "Attach Trigger"
    bl_description = "Attach game trigger/event metadata to selected object"
    bl_options = {'REGISTER', 'UNDO'}

    trigger_name: StringProperty(
        name="Trigger Name",
        description="Name of the trigger",
        default="Trigger_001"
    )

    trigger_type: EnumProperty(
        name="Trigger Type",
        items=[
            ('ON_ENTER', 'On Enter', 'Trigger when object enters'),
            ('ON_EXIT', 'On Exit', 'Trigger when object exits'),
            ('ON_USE', 'On Use', 'Trigger when player uses'),
            ('ON_TOUCH', 'On Touch', 'Trigger when touched'),
            ('ON_PROXIMITY', 'On Proximity', 'Trigger when near'),
            ('ON_TIMER', 'On Timer', 'Trigger after time delay'),
        ],
        default='ON_ENTER'
    )

    event_name: StringProperty(
        name="Event Name",
        description="Game event to trigger",
        default="door_open"
    )

    parameters: StringProperty(
        name="Parameters",
        description="JSON parameters for the event",
        default='{"speed": 1.0, "sound": "door_open.wav"}'
    )

    @classmethod
    def poll(cls, context):
        return context.active_object

    def execute(self, context):
        obj = context.active_object

        # Add trigger metadata
        if not hasattr(obj, 'aeon_triggers'):
            obj['aeon_triggers'] = []

        trigger_data = {
            'name': self.trigger_name,
            'type': self.trigger_type.lower(),
            'event': self.event_name,
            'parameters': json.loads(self.parameters),
            'active': True,
            'cooldown': 0.0
        }

        # Add to object triggers
        triggers = obj.get('aeon_triggers', [])
        triggers.append(trigger_data)
        obj['aeon_triggers'] = triggers

        # Add to scene triggers list
        if not hasattr(context.scene, 'aeon_triggers'):
            context.scene.aeon_triggers = []

        trigger = context.scene.aeon_triggers.add()
        trigger.name = self.trigger_name
        trigger.trigger_type = self.trigger_type
        trigger.event_name = self.event_name
        trigger.parameters = self.parameters

        # Add visual indicator
        self.add_trigger_visual(obj)

        self.report({'INFO'}, f"Attached trigger '{self.trigger_name}' to {obj.name}")
        return {'FINISHED'}

    def add_trigger_visual(self, obj):
        """Add visual indicator for trigger"""
        # Add empty or special material
        if obj.type == 'MESH' and not obj.data.materials:
            mat = bpy.data.materials.new(name="Trigger_Material")
            mat.use_nodes = True
            obj.data.materials.append(mat)

        # Add trigger bounds empty
        bpy.ops.object.empty_add(type='CUBE', location=obj.matrix_world.translation)
        trigger_empty = context.active_object
        trigger_empty.name = f"Trigger_{obj.name}"
        trigger_empty.empty_display_size = 2.0
        trigger_empty.parent = obj
        trigger_empty.hide_render = True

# =====================
# TILE TEMPLATE UI LIST
# =====================

class AEON_UL_TileTemplates(UIList):
    """UI List for tile templates"""
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row()
            row.label(text=item.name, icon='MESH_CUBE')
            if item.tile_object:
                row.label(text=item.tile_object.name, icon='OBJECT_DATA')
            row.label(text=item.category, icon='GROUP')
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon='MESH_CUBE')

# =====================
# MAIN PANEL
# =====================

class AEON_PT_InteriorToolkit(Panel):
    """AEON Interior Toolkit Main Panel"""
    bl_label = "Interior Toolkit"
    bl_idname = "AEON_PT_interior_toolkit"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AEON Tools"

    def draw(self, context):
        layout = self.layout

        # Header
        row = layout.row()
        row.label(text="Tile-Based Interior Tools", icon='HOME')

        # Template Management
        layout.separator()
        box = layout.box()
        box.label(text="Room Templates", icon='FILE')
        col = box.column(align=True)
        col.operator("aeon.create_room_template", icon='NEW')
        col.operator("aeon.place_room_template", icon='MESH_GRID')

        # Tile Painter
        layout.separator()
        box = layout.box()
        box.label(text="Tile Painter", icon='BRUSH_DATA')
        col = box.column(align=True)
        col.operator("aeon.tile_painter", icon='BRUSH_PAINT')

        # Tile Templates UI List
        if hasattr(context.scene, 'aeon_tile_templates'):
            box.label(text="Tile Templates:")
            row = box.row()
            row.template_list(
                "AEON_UL_TileTemplates",
                "",
                context.scene,
                "aeon_tile_templates",
                context.scene,
                "aeon_selected_tile_template_index",
                rows=4,
                maxrows=8
            )

        # Portals
        layout.separator()
        box = layout.box()
        box.label(text="Portals & Transitions", icon='WORLD')
        col = box.column(align=True)
        col.operator("aeon.mark_portal", icon='FORWARD')
        if hasattr(context.scene, 'aeon_portals'):
            col.label(text=f"Portals: {len(context.scene.aeon_portals)}")

        # Quick Views
        layout.separator()
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

        # Triggers & Events
        layout.separator()
        box = layout.box()
        box.label(text="Triggers & Events", icon='HAND')
        col = box.column(align=True)
        col.operator("aeon.attach_trigger", icon='DECORATE_DRIVER')
        if hasattr(context.scene, 'aeon_triggers'):
            col.label(text=f"Triggers: {len(context.scene.aeon_triggers)}")

        # Export
        layout.separator()
        box = layout.box()
        box.label(text="Export Level Data", icon='EXPORT')
        col = box.column(align=True)
        col.operator("aeon.export_level_data", icon='FILE_TEXT')

# =====================
# EXPORT SYSTEM
# =====================

class AEON_OT_ExportLevelData(Operator):
    """Export level data for game engine"""
    bl_idname = "aeon.export_level_data"
    bl_label = "Export Level Data"
    bl_description = "Export level data including tiles, portals, and triggers"
    bl_options = {'REGISTER'}

    export_path: StringProperty(
        name="Export Path",
        description="Path to export level data",
        subtype='FILE_PATH',
        default="//level_data.json"
    )

    export_format: EnumProperty(
        name="Export Format",
        description="Export format",
        items=[
            ('JSON', 'JSON', 'JSON format'),
            ('BINARY', 'Binary', 'Binary format'),
            ('CUSTOM', 'Custom', 'Custom format'),
        ],
        default='JSON'
    )

    include_tiles: BoolProperty(
        name="Include Tiles",
        description="Include tile data",
        default=True
    )

    include_portals: BoolProperty(
        name="Include Portals",
        description="Include portal data",
        default=True
    )

    include_triggers: BoolProperty(
        name="Include Triggers",
        description="Include trigger data",
        default=True
    )

    def execute(self, context):
        level_data = {
            'name': context.scene.name,
            'version': '1.0',
            'tiles': [],
            'portals': [],
            'triggers': [],
            'metadata': {
                'created_by': 'AEON Interior Toolkit',
                'export_time': str(bpy.context.scene.frame_current),
            }
        }

        # Export tiles
        if self.include_tiles:
            self.export_tiles(context, level_data)

        # Export portals
        if self.include_portals and hasattr(context.scene, 'aeon_portals'):
            self.export_portals(context, level_data)

        # Export triggers
        if self.include_triggers and hasattr(context.scene, 'aeon_triggers'):
            self.export_triggers(context, level_data)

        # Save to file
        try:
            with open(bpy.path.abspath(self.export_path), 'w') as f:
                json.dump(level_data, f, indent=2)

            self.report({'INFO'}, f"Exported level data to {self.export_path}")
        except Exception as e:
            self.report({'ERROR'}, f"Export failed: {str(e)}")
            return {'CANCELLED'}

        return {'FINISHED'}

    def export_tiles(self, context, level_data):
        """Export tile data"""
        tiles = []
        grid_size = 1.0  # Could be configurable

        for obj in bpy.data.objects:
            if obj.type == 'MESH' and not obj.parent:
                # Check if object is on grid
                pos = obj.matrix_world.translation
                if (abs(pos.x % grid_size) < 0.001 and
                    abs(pos.y % grid_size) < 0.001):

                    tile_data = {
                        'type': 'tile',
                        'name': obj.name,
                        'position': list(pos),
                        'rotation': list(obj.rotation_euler),
                        'scale': list(obj.scale),
                        'mesh': obj.data.name if obj.data else None,
                    }
                    tiles.append(tile_data)

        level_data['tiles'] = tiles

    def export_portals(self, context, level_data):
        """Export portal data"""
        portals = []
        for portal in context.scene.aeon_portals:
            portal_data = {
                'name': portal.name,
                'target_map': portal.target_map,
                'target_portal': portal.target_portal,
                'type': portal.portal_type,
                'is_bidirectional': portal.is_bidirectional,
                'transition_effect': portal.transition_effect,
                'load_trigger': portal.load_trigger,
            }
            portals.append(portal_data)

        level_data['portals'] = portals

    def export_triggers(self, context, level_data):
        """Export trigger data"""
        triggers = []
        for trigger in context.scene.aeon_triggers:
            trigger_data = {
                'name': trigger.name,
                'type': trigger.trigger_type,
                'event': trigger.event_name,
                'target_object': trigger.target_object,
                'parameters': json.loads(trigger.parameters),
                'is_active': trigger.is_active,
                'cooldown': trigger.cooldown,
            }
            triggers.append(trigger_data)

        level_data['triggers'] = triggers

# =====================
# PLACE ROOM TEMPLATE
# =====================

class AEON_OT_PlaceRoomTemplate(Operator):
    """Place room template in scene"""
    bl_idname = "aeon.place_room_template"
    bl_label = "Place Room Template"
    bl_description = "Place selected room template in scene"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return hasattr(context.scene, 'aeon_room_templates') and len(context.scene.aeon_room_templates) > 0

    def execute(self, context):
        # Get selected template
        templates = getattr(context.scene, 'aeon_room_templates', [])
        if not templates:
            self.report({'WARNING'}, "No room templates available")
            return {'CANCELLED'}

        selected_index = getattr(context.scene, 'aeon_selected_room_template_index', 0)
        if selected_index >= len(templates):
            selected_index = 0

        template = templates[selected_index]

        try:
            template_data = json.loads(template.template_data)
            self.place_template_data(context, template_data)
            self.report({'INFO'}, f"Placed room template: {template.name}")
        except Exception as e:
            self.report({'ERROR'}, f"Failed to place template: {str(e)}")

        return {'FINISHED'}

    def place_template_data(self, context, template_data):
        """Place template data in scene"""
        # Create group for template
        group_name = f"Room_{template_data['name']}"
        if group_name not in bpy.data.collections:
            group = bpy.data.collections.new(group_name)
            context.scene.collection.children.link(group)
        else:
            group = bpy.data.collections[group_name]

        # Place objects from template
        for obj_data in template_data.get('objects', []):
            if obj_data['type'] == 'MESH':
                # Find mesh in scene
                mesh = bpy.data.meshes.get(obj_data['data_path'])
                if mesh:
                    # Create object from mesh
                    obj = bpy.data.objects.new(obj_data['name'], mesh)
                    context.scene.collection.objects.link(obj)

                    # Set transform
                    obj.matrix_world = Matrix.Translation(obj_data['location'])
                    obj.rotation_euler = obj_data['rotation']
                    obj.scale = obj_data['scale']

                    # Add to group
                    group.objects.link(obj)
                    context.scene.collection.objects.unlink(obj)

# =====================
# REGISTRATION
# =====================

def register():
    # Register property groups
    bpy.utils.register_class(AEON_RoomTemplate)
    bpy.utils.register_class(AEON_TileTemplate)
    bpy.utils.register_class(AEON_Portal)
    bpy.utils.register_class(AEON_Trigger)

    # Register UI list
    bpy.utils.register_class(AEON_UL_TileTemplates)

    # Register operators
    bpy.utils.register_class(AEON_OT_CreateRoomTemplate)
    bpy.utils.register_class(AEON_OT_TilePainter)
    bpy.utils.register_class(AEON_OT_MarkPortal)
    bpy.utils.register_class(AEON_OT_QuickView)
    bpy.utils.register_class(AEON_OT_AttachTrigger)
    bpy.utils.register_class(AEON_OT_ExportLevelData)
    bpy.utils.register_class(AEON_OT_PlaceRoomTemplate)

    # Register panel
    bpy.utils.register_class(AEON_PT_InteriorToolkit)

    # Add scene properties
    bpy.types.Scene.aeon_room_templates = bpy.props.CollectionProperty(type=AEON_RoomTemplate)
    bpy.types.Scene.aeon_selected_room_template_index = bpy.props.IntProperty(default=0)
    bpy.types.Scene.aeon_tile_templates = bpy.props.CollectionProperty(type=AEON_TileTemplate)
    bpy.types.Scene.aeon_selected_tile_template_index = bpy.props.IntProperty(default=0)
    bpy.types.Scene.aeon_portals = bpy.props.CollectionProperty(type=AEON_Portal)
    bpy.types.Scene.aeon_triggers = bpy.props.CollectionProperty(type=AEON_Trigger)

    print("AEON Interior Toolkit registered successfully")

def unregister():
    # Unregister panel
    bpy.utils.unregister_class(AEON_PT_InteriorToolkit)

    # Unregister operators
    bpy.utils.unregister_class(AEON_OT_PlaceRoomTemplate)
    bpy.utils.unregister_class(AEON_OT_ExportLevelData)
    bpy.utils.unregister_class(AEON_OT_AttachTrigger)
    bpy.utils.unregister_class(AEON_OT_QuickView)
    bpy.utils.unregister_class(AEON_OT_MarkPortal)
    bpy.utils.unregister_class(AEON_OT_TilePainter)
    bpy.utils.unregister_class(AEON_OT_CreateRoomTemplate)

    # Unregister UI list
    bpy.utils.unregister_class(AEON_UL_TileTemplates)

    # Unregister property groups
    bpy.utils.unregister_class(AEON_Trigger)
    bpy.utils.unregister_class(AEON_Portal)
    bpy.utils.unregister_class(AEON_TileTemplate)
    bpy.utils.unregister_class(AEON_RoomTemplate)

    # Remove scene properties
    del bpy.types.Scene.aeon_triggers
    del bpy.types.Scene.aeon_portals
    del bpy.types.Scene.aeon_selected_tile_template_index
    del bpy.types.Scene.aeon_tile_templates
    del bpy.types.Scene.aeon_selected_room_template_index
    del bpy.types.Scene.aeon_room_templates

    print("AEON Interior Toolkit unregistered")

if __name__ == "__main__":
    register()