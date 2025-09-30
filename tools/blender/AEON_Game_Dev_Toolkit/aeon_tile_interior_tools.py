"""
AEON Tile-Based Interior Tools
Specialized tools for building tile-based interiors efficiently

Author: AEON Engine Development Team
Version: 1.0.0
License: MIT

These tools streamline tile-based interior creation:
- Grid snapping and alignment
- Wall building with automatic connections
- Room layout generation
- Tile placement optimization
"""

bl_info = {
    "name": "AEON Tile Interior Tools",
    "author": "AEON Engine Development Team",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > AEON Tools",
    "description": "Specialized tools for tile-based interior building",
    "category": "Development",
}

import bpy
import bmesh
import mathutils
import math
from mathutils import Vector, Matrix, Quaternion
from bpy.props import (
    BoolProperty, FloatProperty, IntProperty, EnumProperty,
    StringProperty, CollectionProperty, PointerProperty
)
from bpy.types import Panel, Operator, PropertyGroup

# =====================
# TILE GRID SYSTEM
# =====================

class AEON_OT_TileGridSetup(Operator):
    """Set up tile grid for interior building"""
    bl_idname = "aeon.tile_grid_setup"
    bl_label = "Setup Tile Grid"
    bl_description = "Create grid system for tile placement"
    bl_options = {'REGISTER', 'UNDO'}

    grid_size: FloatProperty(
        name="Grid Size",
        description="Size of each tile",
        default=1.0,
        min=0.1,
        max=10.0
    )

    grid_count: IntProperty(
        name="Grid Count",
        description="Number of grid lines",
        default=20,
        min=5,
        max=100
    )

    show_floor: BoolProperty(
        name="Show Floor",
        description="Show floor grid",
        default=True
    )

    floor_height: FloatProperty(
        name="Floor Height",
        description="Height of floor plane",
        default=0.0,
        min=-10.0,
        max=10.0
    )

    def execute(self, context):
        # Create grid object
        grid_name = "Tile_Grid_Helper"

        # Remove existing grid if it exists
        if grid_name in bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects[grid_name], do_unlink=True)

        # Create floor plane if requested
        if self.show_floor:
            floor_size = self.grid_size * self.grid_count
            bpy.ops.mesh.primitive_plane_add(
                size=floor_size,
                location=(0, 0, self.floor_height)
            )
            floor_obj = context.active_object
            floor_obj.name = "Tile_Floor_Reference"
            floor_obj.display_type = 'TEXTURED'
            floor_obj.hide_render = True
            floor_obj.hide_select = True

            # Add grid material
            if not floor_obj.data.materials:
                mat = bpy.data.materials.new(name="Tile_Grid_Material")
                mat.use_nodes = True
                floor_obj.data.materials.append(mat)

        # Create grid lines using empties for visual reference
        grid_group = bpy.data.collections.new("Tile_Grid")
        context.scene.collection.children.link(grid_group)

        # Create grid lines
        half_size = (self.grid_size * self.grid_count) / 2

        for i in range(-self.grid_count//2, self.grid_count//2 + 1):
            pos = i * self.grid_size

            # X-axis lines
            bpy.ops.object.empty_add(
                type='PLAIN_AXES',
                location=(pos, 0, self.floor_height + 0.01),
                scale=(self.grid_size/2, 0, 0)
            )
            line_x = context.active_object
            line_x.name = f"Grid_X_{i}"
            line_x.empty_display_size = 0.01
            grid_group.objects.link(line_x)
            context.scene.collection.objects.unlink(line_x)

            # Y-axis lines
            bpy.ops.object.empty_add(
                type='PLAIN_AXES',
                location=(0, pos, self.floor_height + 0.01),
                scale=(0, self.grid_size/2, 0)
            )
            line_y = context.active_object
            line_y.name = f"Grid_Y_{i}"
            line_y.empty_display_size = 0.01
            grid_group.objects.link(line_y)
            context.scene.collection.objects.unlink(line_y)

        # Set up grid and snapping
        context.scene.tool_settings.use_snap = True
        context.scene.tool_settings.snap_elements = {'INCREMENT'}
        context.scene.tool_settings.snap_target = 'CLOSEST'

        self.report({'INFO'}, f"Tile grid setup: {self.grid_size}m spacing, {self.grid_count}x{self.grid_count} grid")
        return {'FINISHED'}

# =====================
# TILE SNAPPING SYSTEM
# =====================

class AEON_OT_TileSnapper(Operator):
    """Snap selected objects to tile grid"""
    bl_idname = "aeon.tile_snapper"
    bl_label = "Snap to Grid"
    bl_description = "Snap selected objects to tile grid"
    bl_options = {'REGISTER', 'UNDO'}

    grid_size: FloatProperty(
        name="Grid Size",
        description="Tile grid size",
        default=1.0,
        min=0.1,
        max=10.0
    )

    snap_to_origin: BoolProperty(
        name="Snap to Origin",
        description="Snap object origins instead of geometry",
        default=True
    )

    def execute(self, context):
        selected_objects = [obj for obj in context.selected_objects if obj.type == 'MESH']

        if not selected_objects:
            self.report({'WARNING'}, "No mesh objects selected")
            return {'CANCELLED'}

        processed_count = 0

        for obj in selected_objects:
            if self.snap_to_origin:
                # Snap object origin
                loc = obj.matrix_world.translation
                snapped_loc = Vector((
                    round(loc.x / self.grid_size) * self.grid_size,
                    round(loc.y / self.grid_size) * self.grid_size,
                    round(loc.z / self.grid_size) * self.grid_size
                ))
                obj.matrix_world.translation = snapped_loc
            else:
                # Snap geometry (more complex - need to adjust vertices)
                me = obj.data
                bm = bmesh.new()
                bm.from_mesh(me)

                for vert in bm.verts:
                    world_pos = obj.matrix_world @ vert.co
                    snapped_world = Vector((
                        round(world_pos.x / self.grid_size) * self.grid_size,
                        round(world_pos.y / self.grid_size) * self.grid_size,
                        round(world_pos.z / self.grid_size) * self.grid_size
                    ))
                    vert.co = obj.matrix_world.inverted() @ snapped_world

                bm.to_mesh(me)
                bm.free()
                me.update()

            processed_count += 1

        self.report({'INFO'}, f"Snapped {processed_count} objects to grid")
        return {'FINISHED'}

# =====================
# WALL BUILDER TOOLS
# =====================

class AEON_OT_WallBuilder(Operator):
    """Build walls along path or between points"""
    bl_idname = "aeon.wall_builder"
    bl_label = "Build Wall"
    bl_description = "Build wall along selected path or between points"
    bl_options = {'REGISTER', 'UNDO'}

    wall_template: PointerProperty(
        name="Wall Template",
        description="Template object for wall segments",
        type=bpy.types.Object
    )

    wall_height: FloatProperty(
        name="Wall Height",
        description="Height of wall segments",
        default=3.0,
        min=0.1,
        max=20.0
    )

    auto_connect: BoolProperty(
        name="Auto Connect",
        description="Automatically connect wall segments",
        default=True
    )

    add_corners: BoolProperty(
        name="Add Corner Pieces",
        description="Add corner pieces at wall junctions",
        default=True
    )

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type in ['MESH', 'CURVE']

    def execute(self, context):
        if not self.wall_template:
            self.report({'WARNING'}, "Please select a wall template object")
            return {'CANCELLED'}

        active_obj = context.active_object
        wall_segments = []

        if active_obj.type == 'CURVE':
            # Build wall along curve
            wall_segments = self.build_wall_from_curve(active_obj, self.wall_template, self.wall_height)
        else:
            # Build wall from mesh (use bounding box or selected edges)
            wall_segments = self.build_wall_from_mesh(active_obj, self.wall_template, self.wall_height)

        # Group wall segments
        if wall_segments:
            wall_group = bpy.data.collections.new(f"Wall_System_{active_obj.name}")
            context.scene.collection.children.link(wall_group)

            for segment in wall_segments:
                wall_group.objects.link(segment)
                context.scene.collection.objects.unlink(segment)

        self.report({'INFO'}, f"Built {len(wall_segments)} wall segments")
        return {'FINISHED'}

    def build_wall_from_curve(self, curve_obj, template, height):
        """Build wall segments along curve"""
        segments = []

        # Sample curve points
        curve_length = self.get_curve_length(curve_obj)
        segment_length = template.dimensions.x  # Assume X is length
        num_segments = int(curve_length / segment_length)

        for i in range(num_segments):
            # Get position along curve
            t = i / max(1, num_segments - 1)
            position = self.get_point_on_curve(curve_obj, t)
            tangent = self.get_curve_tangent(curve_obj, t)

            # Calculate rotation
            forward = tangent.normalized()
            up = Vector((0, 0, 1))
            right = forward.cross(up).normalized()
            up = right.cross(forward)

            rotation = Matrix((right, up, -forward)).transposed().to_3x3().to_quaternion()

            # Duplicate template
            segment = self.duplicate_with_transform(template, position, rotation, height)
            segments.append(segment)

        return segments

    def build_wall_from_mesh(self, mesh_obj, template, height):
        """Build wall segments from mesh reference"""
        segments = []

        # Get mesh bounds and use to determine wall path
        bbox_corners = [mesh_obj.matrix_world @ Vector(corner) for corner in mesh_obj.bound_box]

        # Simple rectangular wall based on bounding box
        min_x = min(corner.x for corner in bbox_corners)
        max_x = max(corner.x for corner in bbox_corners)
        min_y = min(corner.y for corner in bbox_corners)
        max_y = max(corner.y for corner in bbox_corners)

        segment_length = template.dimensions.x

        # Build perimeter walls
        # South wall
        x = min_x
        while x < max_x:
            pos = Vector((x + segment_length/2, min_y, 0))
            segment = self.duplicate_with_transform(template, pos, Quaternion((0, 0, 0, 1)), height)
            segments.append(segment)
            x += segment_length

        # North wall
        x = min_x
        while x < max_x:
            pos = Vector((x + segment_length/2, max_y, 0))
            segment = self.duplicate_with_transform(template, pos, Quaternion((0, 0, 0, 1)), height)
            segments.append(segment)
            x += segment_length

        # East wall
        y = min_y + segment_length
        while y < max_y:
            pos = Vector((max_x, y, 0))
            segment = self.duplicate_with_transform(template, pos, Quaternion((0, 0, 0.707, 0.707)), height)
            segments.append(segment)
            y += segment_length

        # West wall
        y = min_y + segment_length
        while y < max_y:
            pos = Vector((min_x, y, 0))
            segment = self.duplicate_with_transform(template, pos, Quaternion((0, 0, 0.707, 0.707)), height)
            segments.append(segment)
            y += segment_length

        return segments

    def duplicate_with_transform(self, template, position, rotation, height):
        """Duplicate template with transforms"""
        # Store selection
        original_selection = context.selected_objects.copy()

        # Select and duplicate template
        bpy.ops.object.select_all(action='DESELECT')
        template.select_set(True)
        context.view_layer.objects.active = template

        bpy.ops.object.duplicate()
        segment = context.active_object

        # Apply transforms
        segment.matrix_world.translation = position
        segment.rotation_mode = 'QUATERNION'
        segment.rotation_quaternion = rotation

        # Scale to height if needed
        if height > 0 and segment.dimensions.z > 0:
            scale_factor = height / segment.dimensions.z
            segment.scale.z = scale_factor

        # Restore selection
        bpy.ops.object.select_all(action='DESELECT')
        for obj in original_selection:
            obj.select_set(True)
        if original_selection:
            context.view_layer.objects.active = original_selection[0]

        return segment

    def get_curve_length(self, curve_obj):
        """Calculate approximate curve length"""
        if curve_obj.type != 'CURVE':
            return 0

        spline = curve_obj.data.splines[0] if curve_obj.data.splines else None
        if not spline:
            return 0

        if spline.type == 'POLY':
            length = 0
            points = spline.bezier_points if hasattr(spline, 'bezier_points') else spline.points
            for i in range(len(points) - 1):
                p1 = points[i].co
                p2 = points[i + 1].co
                length += (p2 - p1).length
            return length
        else:
            # Approximate for other curve types
            return spline.calc_length()

    def get_point_on_curve(self, curve_obj, t):
        """Get point on curve at parameter t (0-1)"""
        if curve_obj.type != 'CURVE':
            return Vector()

        spline = curve_obj.data.splines[0] if curve_obj.data.splines else None
        if not spline:
            return Vector()

        if spline.type == 'POLY':
            points = spline.bezier_points if hasattr(spline, 'bezier_points') else spline.points
            total_length = self.get_curve_length(curve_obj)
            target_length = t * total_length

            current_length = 0
            for i in range(len(points) - 1):
                p1 = points[i].co
                p2 = points[i + 1].co
                segment_length = (p2 - p1).length

                if current_length + segment_length >= target_length:
                    # Point is on this segment
                    segment_t = (target_length - current_length) / segment_length
                    return curve_obj.matrix_world @ (p1.lerp(p2, segment_t))

                current_length += segment_length

            return curve_obj.matrix_world @ points[-1].co
        else:
            # Approximate for other curve types
            return curve_obj.matrix_world @ spline.point_normals

    def get_curve_tangent(self, curve_obj, t):
        """Get tangent direction on curve at parameter t"""
        # Simplified - return forward direction
        return Vector((1, 0, 0))

# =====================
# ROOM LAYOUT TOOLS
# =====================

class AEON_OT_RoomLayoutGenerator(Operator):
    """Generate room layouts from templates"""
    bl_idname = "aeon.room_layout_generator"
    bl_label = "Generate Room Layout"
    bl_description = "Generate complete room layouts from templates"
    bl_options = {'REGISTER', 'UNDO'}

    room_type: EnumProperty(
        name="Room Type",
        description="Type of room to generate",
        items=[
            ('SQUARE', 'Square Room', 'Basic square room'),
            ('RECTANGLE', 'Rectangle', 'Rectangular room'),
            ('L_SHAPE', 'L-Shaped', 'L-shaped room'),
            ('CROSS', 'Cross Shape', 'Cross-shaped room'),
            ('CORRIDOR', 'Corridor', 'Long corridor'),
        ],
        default='SQUARE'
    )

    room_width: IntProperty(
        name="Width (tiles)",
        description="Room width in tiles",
        default=10,
        min=3,
        max=50
    )

    room_length: IntProperty(
        name="Length (tiles)",
        description="Room length in tiles",
        default=10,
        min=3,
        max=50
    )

    include_doors: BoolProperty(
        name="Include Doors",
        description="Add door openings",
        default=True
    )

    include_corners: BoolProperty(
        name="Include Corners",
        description="Add corner pieces",
        default=True
    )

    floor_template: PointerProperty(
        name="Floor Template",
        description="Template for floor tiles",
        type=bpy.types.Object
    )

    wall_template: PointerProperty(
        name="Wall Template",
        description="Template for wall segments",
        type=bpy.types.Object
    )

    door_template: PointerProperty(
        name="Door Template",
        description="Template for door frames",
        type=bpy.types.Object
    )

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if not self.floor_template or not self.wall_template:
            self.report({'WARNING'}, "Please select floor and wall templates")
            return {'CANCELLED'}

        # Generate room layout
        room_objects = self.generate_room_layout(
            self.room_type,
            self.room_width,
            self.room_length,
            self.floor_template,
            self.wall_template,
            self.door_template if self.include_doors else None,
            self.include_corners
        )

        # Group room objects
        if room_objects:
            room_group = bpy.data.collections.new(f"Room_{self.room_type}_{self.room_width}x{self.room_length}")
            context.scene.collection.children.link(room_group)

            for obj in room_objects:
                room_group.objects.link(obj)
                context.scene.collection.objects.unlink(obj)

        self.report({'INFO'}, f"Generated {self.room_type} room with {len(room_objects)} objects")
        return {'FINISHED'}

    def generate_room_layout(self, room_type, width, length, floor_template, wall_template, door_template, include_corners):
        """Generate complete room layout"""
        objects = []
        grid_size = 1.0  # Assume 1 unit grid

        # Generate floor
        for x in range(width):
            for y in range(length):
                pos = Vector((
                    (x - width/2 + 0.5) * grid_size,
                    (y - length/2 + 0.5) * grid_size,
                    0
                ))
                floor_tile = self.duplicate_object(floor_template, pos)
                objects.append(floor_tile)

        # Generate walls based on room type
        if room_type == 'SQUARE':
            walls = self.generate_square_walls(width, length, wall_template, door_template, grid_size)
        elif room_type == 'RECTANGLE':
            walls = self.generate_rectangle_walls(width, length, wall_template, door_template, grid_size)
        elif room_type == 'L_SHAPE':
            walls = self.generate_l_shape_walls(width, length, wall_template, door_template, grid_size)
        elif room_type == 'CROSS':
            walls = self.generate_cross_walls(width, length, wall_template, door_template, grid_size)
        elif room_type == 'CORRIDOR':
            walls = self.generate_corridor_walls(width, length, wall_template, door_template, grid_size)

        objects.extend(walls)

        # Add corner pieces if requested
        if include_corners and walls:
            corners = self.generate_corner_pieces(walls, grid_size)
            objects.extend(corners)

        return objects

    def generate_square_walls(self, width, length, wall_template, door_template, grid_size):
        """Generate walls for square room"""
        walls = []

        # Door positions (simplified - place doors in middle of walls)
        door_positions = []
        if door_template:
            door_positions = [
                (width // 2, 0),  # South door
                (width, length // 2),  # East door
                (width // 2, length),  # North door
                (0, length // 2),  # West door
            ]

        # South wall
        for x in range(width):
            if (x, 0) not in door_positions:
                pos = Vector((x - width/2 + 0.5, -length/2, 0))
                wall = self.duplicate_object(wall_template, pos)
                walls.append(wall)

        # North wall
        for x in range(width):
            if (x, length) not in door_positions:
                pos = Vector((x - width/2 + 0.5, length/2, 0))
                wall = self.duplicate_object(wall_template, pos)
                walls.append(wall)

        # East wall
        for y in range(length):
            if (width, y) not in door_positions:
                pos = Vector((width/2, y - length/2 + 0.5, 0))
                wall = self.duplicate_object(wall_template, pos)
                wall.rotation_euler.z = math.radians(90)
                walls.append(wall)

        # West wall
        for y in range(length):
            if (0, y) not in door_positions:
                pos = Vector((-width/2, y - length/2 + 0.5, 0))
                wall = self.duplicate_object(wall_template, pos)
                wall.rotation_euler.z = math.radians(90)
                walls.append(wall)

        # Add doors
        if door_template:
            for door_x, door_y in door_positions:
                pos = Vector((door_x - width/2, door_y - length/2, 0))
                door = self.duplicate_object(door_template, pos)
                # Orient door correctly
                if door_x == 0 or door_x == width:
                    door.rotation_euler.z = math.radians(90)
                walls.append(door)

        return walls

    def generate_rectangle_walls(self, width, length, wall_template, door_template, grid_size):
        """Generate walls for rectangular room (same as square for now)"""
        return self.generate_square_walls(width, length, wall_template, door_template, grid_size)

    def generate_l_shape_walls(self, width, length, wall_template, door_template, grid_size):
        """Generate L-shaped room walls"""
        # Simplified L-shape
        walls = []

        # This would generate more complex L-shaped layouts
        # For now, return basic implementation
        return self.generate_square_walls(width//2, length, wall_template, door_template, grid_size)

    def generate_cross_walls(self, width, length, wall_template, door_template, grid_size):
        """Generate cross-shaped room walls"""
        walls = []

        # Simplified cross shape
        # This would generate more complex cross-shaped layouts
        return self.generate_square_walls(width, length, wall_template, door_template, grid_size)

    def generate_corridor_walls(self, width, length, wall_template, door_template, grid_size):
        """Generate corridor walls"""
        walls = []

        # Narrow corridor
        corridor_width = 3

        # Side walls
        for y in range(length):
            # Left wall
            pos = Vector((-corridor_width/2, y - length/2 + 0.5, 0))
            wall = self.duplicate_object(wall_template, pos)
            wall.rotation_euler.z = math.radians(90)
            walls.append(wall)

            # Right wall
            pos = Vector((corridor_width/2, y - length/2 + 0.5, 0))
            wall = self.duplicate_object(wall_template, pos)
            wall.rotation_euler.z = math.radians(90)
            walls.append(wall)

        return walls

    def generate_corner_pieces(self, walls, grid_size):
        """Generate corner pieces at wall intersections"""
        corners = []
        # Simplified corner generation
        # This would analyze wall intersections and place appropriate corner pieces
        return corners

    def duplicate_object(self, template, position):
        """Duplicate template at position"""
        # Store selection
        original_selection = context.selected_objects.copy()

        # Select and duplicate template
        bpy.ops.object.select_all(action='DESELECT')
        template.select_set(True)
        context.view_layer.objects.active = template

        bpy.ops.object.duplicate()
        duplicate = context.active_object
        duplicate.matrix_world.translation = position

        # Restore selection
        bpy.ops.object.select_all(action='DESELECT')
        for obj in original_selection:
            obj.select_set(True)
        if original_selection:
            context.view_layer.objects.active = original_selection[0]

        return duplicate

# =====================
# TILE PLACEMENT TOOLS
# =====================

class AEON_OT_TilePlacer(Operator):
    """Intelligent tile placement with auto-alignment"""
    bl_idname = "aeon.tile_placer"
    bl_label = "Place Tile"
    bl_description = "Place tile with intelligent alignment and connection"
    bl_options = {'REGISTER', 'UNDO'}

    tile_template: PointerProperty(
        name="Tile Template",
        description="Template object to place",
        type=bpy.types.Object
    )

    auto_align: BoolProperty(
        name="Auto Align",
        description="Automatically align with nearby tiles",
        default=True
    )

    snap_to_grid: BoolProperty(
        name="Snap to Grid",
        description="Snap to tile grid",
        default=True
    )

    connect_edges: BoolProperty(
        name="Connect Edges",
        description="Connect to adjacent tiles",
        default=True
    )

    grid_size: FloatProperty(
        name="Grid Size",
        description="Tile grid size",
        default=1.0,
        min=0.1,
        max=10.0
    )

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH'

    def execute(self, context):
        if not self.tile_template:
            self.report({'WARNING'}, "Please select a tile template")
            return {'CANCELLED'}

        # Get placement position (could be from 3D cursor, selected object, etc.)
        placement_pos = context.scene.cursor.location.copy()

        if self.snap_to_grid:
            placement_pos = Vector((
                round(placement_pos.x / self.grid_size) * self.grid_size,
                round(placement_pos.y / self.grid_size) * self.grid_size,
                round(placement_pos.z / self.grid_size) * self.grid_size
            ))

        # Place tile
        new_tile = self.duplicate_object(self.tile_template, placement_pos)

        # Auto-align with nearby tiles
        if self.auto_align:
            self.align_with_nearby_tiles(new_tile, context.selected_objects)

        self.report({'INFO'}, f"Placed tile at {placement_pos}")
        return {'FINISHED'}

    def align_with_nearby_tiles(self, new_tile, nearby_objects):
        """Align new tile with nearby tiles"""
        # This would analyze nearby tiles and adjust rotation/connection
        # Simplified for now
        pass

    def duplicate_object(self, template, position):
        """Duplicate template at position"""
        # Store selection
        original_selection = context.selected_objects.copy()

        # Select and duplicate template
        bpy.ops.object.select_all(action='DESELECT')
        template.select_set(True)
        context.view_layer.objects.active = template

        bpy.ops.object.duplicate()
        duplicate = context.active_object
        duplicate.matrix_world.translation = position

        # Restore selection
        bpy.ops.object.select_all(action='DESELECT')
        for obj in original_selection:
            obj.select_set(True)
        if original_selection:
            context.view_layer.objects.active = original_selection[0]

        return duplicate

# =====================
# TILE INTERIOR PANEL
# =====================

class AEON_PT_TileInteriorTools(Panel):
    """AEON Tile-Based Interior Tools Panel"""
    bl_label = "Tile Interior Tools"
    bl_idname = "AEON_PT_tile_interior_tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AEON Tools"

    def draw(self, context):
        layout = self.layout

        # Header
        row = layout.row()
        row.label(text="Tile-Based Interior Tools", icon='SNAP_GRID')

        # Grid Setup
        layout.separator()
        box = layout.box()
        box.label(text="Grid Setup", icon='GRID')
        col = box.column(align=True)
        col.operator("aeon.tile_grid_setup", icon='MESH_GRID')

        # Tile Placement
        layout.separator()
        box = layout.box()
        box.label(text="Tile Placement", icon='SNAP_SURFACE')
        col = box.column(align=True)
        col.operator("aeon.tile_placer", icon='ADD')
        col.operator("aeon.tile_snapper", icon='SNAP_ON')

        # Wall Building
        layout.separator()
        box = layout.box()
        box.label(text="Wall Building", icon='MESH_CUBE')
        col = box.column(align=True)
        col.operator("aeon.wall_builder", icon='MOD_BUILD')

        # Room Generation
        layout.separator()
        box = layout.box()
        box.label(text="Room Generation", icon='HOME')
        col = box.column(align=True)
        col.operator("aeon.room_layout_generator", icon='MOD_ARRAY')

        # Tips
        layout.separator()
        box = layout.box()
        box.label(text="Tips", icon='INFO')
        col = box.column(align=True)
        col.label(text="• Create tile templates first")
        col.label(text="• Use grid for precise alignment")
        col.label(text="• Wall builder works with curves")
        col.label(text="• Room gen needs templates")

# Registration
classes = (
    # Grid System
    AEON_OT_TileGridSetup,

    # Snapping
    AEON_OT_TileSnapper,

    # Wall Building
    AEON_OT_WallBuilder,

    # Room Layout
    AEON_OT_RoomLayoutGenerator,

    # Tile Placement
    AEON_OT_TilePlacer,

    # Panel
    AEON_PT_TileInteriorTools,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    print("AEON Tile Interior Tools registered successfully")

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    print("AEON Tile Interior Tools unregistered")

if __name__ == "__main__":
    register()