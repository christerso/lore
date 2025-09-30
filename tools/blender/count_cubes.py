"""
Blender script to count cube objects in the current scene
Run this in Blender's Text Editor or Scripting workspace
"""
import bpy

def count_cubes():
    """Count all cube objects in the current scene"""
    scene_objects = bpy.context.scene.objects
    total_objects = len(scene_objects)

    cube_count = 0
    cube_objects = []

    print(f"\n=== CUBE COUNTING REPORT ===")
    print(f"Total objects in scene: {total_objects}")
    print(f"Analyzing objects for cubes...\n")

    for obj in scene_objects:
        is_cube = False
        cube_type = ""

        # Method 1: Check if object is named 'Cube' or contains 'Cube'
        if 'Cube' in obj.name or obj.name.lower() == 'cube':
            is_cube = True
            cube_type = "named cube"

        # Method 2: Check mesh objects that might be cubes
        elif obj.type == 'MESH' and obj.data:
            # Check if it has 8 vertices (typical for a cube)
            if len(obj.data.vertices) == 8:
                # Additional check for cube-like geometry
                # A cube should have 6 faces and 12 edges
                if len(obj.data.polygons) == 6 and len(obj.data.edges) == 12:
                    is_cube = True
                    cube_type = "mesh cube (8 verts, 6 faces, 12 edges)"
                else:
                    cube_type = f"8-vertex mesh (but {len(obj.data.polygons)} faces, {len(obj.data.edges)} edges)"

        if is_cube:
            cube_count += 1
            cube_objects.append((obj.name, cube_type))
            print(f"✓ CUBE FOUND: '{obj.name}' - {cube_type}")
        else:
            print(f"  Object: '{obj.name}' - Type: {obj.type}" +
                  (f" ({len(obj.data.vertices)} verts)" if obj.type == 'MESH' and obj.data else ""))

    print(f"\n=== RESULTS ===")
    print(f"🎲 CUBE COUNT: {cube_count}")

    if cube_objects:
        print(f"\nCube objects found:")
        for i, (name, cube_type) in enumerate(cube_objects, 1):
            print(f"  {i}. '{name}' - {cube_type}")
    else:
        print("No cube objects found in the scene")

    print(f"\n=== END REPORT ===\n")

    return cube_count, cube_objects

# Run the function
if __name__ == "__main__":
    count_cubes()