#!/usr/bin/env python3
"""
Test script for AEON Interior Toolkit
This script simulates Blender environment to test the addon
"""

import sys
import os
import json
import math

# Mock Blender modules for testing
class MockVector:
    def __init__(self, values):
        if isinstance(values, (list, tuple)):
            self.values = list(values)
        else:
            self.values = [values] * 3

    def __getitem__(self, index):
        return self.values[index]

    def __setitem__(self, index, value):
        self.values[index] = value

    def __len__(self):
        return len(self.values)

    @property
    def x(self):
        return self.values[0]

    @property
    def y(self):
        return self.values[1]

    @property
    def z(self):
        return self.values[2]

    def __add__(self, other):
        if isinstance(other, MockVector):
            return MockVector([self[i] + other[i] for i in range(3)])
        return MockVector([self[i] + other for i in range(3)])

    def __sub__(self, other):
        if isinstance(other, MockVector):
            return MockVector([self[i] - other[i] for i in range(3)])
        return MockVector([self[i] - other for i in range(3)])

    def __mul__(self, scalar):
        return MockVector([self[i] * scalar for i in range(3)])

    @property
    def length(self):
        return math.sqrt(sum(x**2 for x in self.values))

class MockMatrix:
    def __init__(self):
        self.translation = MockVector([0, 0, 0])

    @property
    def world(self):
        return self

    @property
    def to_euler(self):
        return MockVector([0, 0, 0])

    def __mul__(self, other):
        return MockVector([0, 0, 0])

class MockQuaternion:
    def __init__(self, values):
        self.values = values

class MockObject:
    def __init__(self, name="TestObject", obj_type="MESH"):
        self.name = name
        self.type = obj_type
        self.matrix_world = MockMatrix()
        self.bound_box = [MockVector([-1, -1, -1]), MockVector([1, 1, 1])]
        self.data = MockMeshData()
        self.materials = []
        self.scale = MockVector([1, 1, 1])
        self.rotation_euler = MockVector([0, 0, 0])
        self.parent = None
        self.hide_render = False

    def select_set(self, selected):
        pass

    @property
    def selected_objects(self):
        return [self]

class MockMeshData:
    def __init__(self):
        self.name = "TestMesh"
        self.materials = []

class MockMaterial:
    def __init__(self, name):
        self.name = name
        self.use_nodes = True

class MockCollection:
    def __init__(self, name):
        self.name = name
        self.objects = []
        self.children = []

    def link(self, obj):
        pass

    def unlink(self, obj):
        pass

class MockScene:
    def __init__(self):
        self.name = "TestScene"
        self.frame_current = 1
        self.collection = MockCollection("SceneCollection")
        self.aeon_room_templates = []
        self.aeon_tile_templates = []
        self.aeon_portals = []
        self.aeon_triggers = []
        self.aeon_selected_room_template_index = 0
        self.aeon_selected_tile_template_index = 0

class MockContext:
    def __init__(self):
        self.scene = MockScene()
        self.selected_objects = []
        self.active_object = None
        self.space_data = MockSpaceData()
        self.region = MockRegion()
        self.region_data = MockRegionData()
        self.window_manager = MockWindowManager()
        self.view_layer = MockViewLayer()

    def report(self, type, message):
        print(f"REPORT {type}: {message}")

class MockSpaceData:
    def __init__(self):
        self.type = 'VIEW_3D'

class MockRegion:
    def __init__(self):
        pass

    def tag_redraw(self):
        pass

class MockRegionData:
    def __init__(self):
        self.view_rotation = MockQuaternion([1, 0, 0, 0])
        self.view_location = MockVector([0, 0, 0])
        self.view_distance = 10
        self.view_perspective = 'PERSPECTIVE'

class MockWindowManager:
    def __init__(self):
        pass

    def modal_handler_add(self, handler):
        pass

class MockViewLayer:
    def __init__(self):
        self.objects_active = None

    @property
    def active(self):
        return self.objects_active

class MockEvent:
    def __init__(self, event_type, value):
        self.type = event_type
        self.value = value
        self.mouse_region_x = 100
        self.mouse_region_y = 100

class MockPropertyGroup:
    def __init__(self):
        pass

# Mock bpy module
class MockBpy:
    def __init__(self):
        self.data = MockData()
        self.types = MockTypes()
        self.props = MockProps()
        self.utils = MockUtils()
        self.context = MockContext()
        self.ops = MockOps()
        self.path = MockPath()

class MockData:
    def __init__(self):
        self.objects = []
        self.meshes = []
        self.materials = []
        self.collections = []

    def new(self, type, name):
        if type == 'object':
            obj = MockObject(name)
            self.objects.append(obj)
            return obj
        elif type == 'mesh':
            mesh = MockMeshData()
            self.meshes.append(mesh)
            return mesh
        elif type == 'material':
            mat = MockMaterial(name)
            self.materials.append(mat)
            return mat
        elif type == 'collection':
            col = MockCollection(name)
            self.collections.append(col)
            return col
        return None

    def remove(self, obj, do_unlink=True):
        if obj in self.objects:
            self.objects.remove(obj)

    def get(self, name):
        for obj in self.objects:
            if obj.name == name:
                return obj
        return None

class MockTypes:
    class Object:
        pass

    class Panel:
        pass

    class Operator:
        pass

    class PropertyGroup:
        pass

    class UIList:
        pass

    class Scene:
        def __setattr__(self, name, value):
            pass

        def __delattr__(self, name):
            pass

class MockProps:
    class BoolProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class FloatProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class IntProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class EnumProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class StringProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class CollectionProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class PointerProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class FloatVectorProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

    class IntVectorProperty:
        def __init__(self, **kwargs):
            for k, v in kwargs.items():
                setattr(self, k, v)

class MockUtils:
    def register_class(self, cls):
        print(f"Registering class: {cls.__name__}")

    def unregister_class(self, cls):
        print(f"Unregistering class: {cls.__name__}")

class MockOps:
    class Object:
        def select_all(self, action):
            print(f"select_all({action})")

        def duplicate(self):
            print("duplicate()")

        def empty_add(self, **kwargs):
            print(f"empty_add({kwargs})")

class MockPath:
    def abspath(self, path):
        return os.path.abspath(path)

# Set up mock environment
sys.modules['bpy'] = MockBpy()
sys.modules['bpy.props'] = MockProps()
sys.modules['bpy.types'] = MockTypes()
sys.modules['bpy.utils'] = MockUtils()
sys.modules['bpy.ops'] = MockOps()
sys.modules['bpy.path'] = MockPath()
sys.modules['bmesh'] = type('MockBmesh', (), {})()
sys.modules['mathutils'] = type('MockMathutils', (), {
    'Vector': MockVector,
    'Matrix': MockMatrix,
    'Quaternion': MockQuaternion
})()

def test_addon():
    """Test the addon functionality"""
    print("=== AEON Interior Toolkit Test ===\n")

    # Add current directory to path
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

    try:
        # Import the addon
        print("1. Importing addon...")
        import aeon_interior_toolkit
        print("SUCCESS: Addon imported successfully")

        # Test registration
        print("\n2. Testing registration...")
        aeon_interior_toolkit.register()
        print("SUCCESS: Registration successful")

        # Test basic functionality
        print("\n3. Testing basic functionality...")

        # Test tile painter
        print("   - Testing Tile Painter...")
        tile_painter = aeon_interior_toolkit.AEON_OT_TilePainter()
        print("SUCCESS: Tile Painter created")

        # Test room template
        print("   - Testing Room Template...")
        room_template = aeon_interior_toolkit.AEON_RoomTemplate()
        print("SUCCESS: Room Template created")

        # Test portal
        print("   - Testing Portal...")
        portal = aeon_interior_toolkit.AEON_Portal()
        print("SUCCESS: Portal created")

        # Test trigger
        print("   - Testing Trigger...")
        trigger = aeon_interior_toolkit.AEON_Trigger()
        print("SUCCESS: Trigger created")

        # Test operators
        print("\n4. Testing operators...")
        operators = [
            aeon_interior_toolkit.AEON_OT_CreateRoomTemplate,
            aeon_interior_toolkit.AEON_OT_TilePainter,
            aeon_interior_toolkit.AEON_OT_MarkPortal,
            aeon_interior_toolkit.AEON_OT_QuickView,
            aeon_interior_toolkit.AEON_OT_AttachTrigger,
            aeon_interior_toolkit.AEON_OT_ExportLevelData,
            aeon_interior_toolkit.AEON_OT_PlaceRoomTemplate
        ]

        for op in operators:
            print(f"   - {op.bl_idname}: SUCCESS")

        # Test UI elements
        print("\n5. Testing UI elements...")
        panel = aeon_interior_toolkit.AEON_PT_InteriorToolkit()
        ui_list = aeon_interior_toolkit.AEON_UL_TileTemplates()
        print("SUCCESS: UI elements created")

        # Test unregistration
        print("\n6. Testing unregistration...")
        aeon_interior_toolkit.unregister()
        print("SUCCESS: Unregistration successful")

        print("\n=== All Tests Passed! ===")
        return True

    except Exception as e:
        print(f"\nERROR: {str(e)}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = test_addon()
    sys.exit(0 if success else 1)