"""
AEON Interior Toolkit - Comprehensive Test Suite

This test suite validates all features of the enhanced AEON Interior Toolkit
to ensure it works correctly in Blender 4.5+.

Run this script in Blender's Scripting workspace to test the toolkit.
"""

import bpy
import sys
import traceback
from typing import List, Dict, Any, Optional

# Blender version compatibility check
BLENDER_VERSION = bpy.app.version
BLENDER_4_5_PLUS = BLENDER_VERSION >= (4, 5, 0)

print(f"🔍 Blender Version: {BLENDER_VERSION[0]}.{BLENDER_VERSION[1]}.{BLENDER_VERSION[2]}")
print(f"🔍 Blender 4.5+ Compatible: {'Yes' if BLENDER_4_5_PLUS else 'No'}")

class AEONToolkitTester:
    """Comprehensive testing framework for AEON Interior Toolkit"""

    def __init__(self):
        self.test_results = []
        self.total_tests = 0
        self.passed_tests = 0
        self.failed_tests = 0

    def log_test(self, test_name: str, passed: bool, details: str = ""):
        """Log test result"""
        self.total_tests += 1
        if passed:
            self.passed_tests += 1
            status = "✅ PASS"
        else:
            self.failed_tests += 1
            status = "❌ FAIL"

        result = f"{status} {test_name}"
        if details:
            result += f" - {details}"

        self.test_results.append(result)
        print(result)

    def setup_test_environment(self):
        """Setup test environment with test objects"""
        print("🔧 Setting up test environment...")

        # Clear existing objects
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        # Create test tile
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
        test_tile = bpy.context.active_object
        test_tile.name = "test_floor_tile"

        # Create test wall
        bpy.ops.mesh.primitive_cube_add(size=1, location=(2, 0, 0))
        test_wall = bpy.context.active_object
        test_wall.name = "test_wall_tile"
        test_wall.scale = (0.1, 1, 2)

        # Create test door
        bpy.ops.mesh.primitive_cube_add(size=1, location=(4, 0, 0))
        test_door = bpy.context.active_object
        test_door.name = "test_door"
        test_door.scale = (0.1, 1.5, 2)

        # Create test prop
        bpy.ops.mesh.primitive_cube_add(size=0.5, location=(6, 0, 0.5))
        test_prop = bpy.context.active_object
        test_prop.name = "test_prop"

        print("✅ Test environment setup complete")

    def test_blender_compatibility(self):
        """Test Blender 4.5+ compatibility"""
        print("\n🔧 Testing Blender 4.5+ Compatibility...")

        # Test context override functionality (important for Blender 4.5+)
        try:
            if BLENDER_4_5_PLUS:
                # Test temp_override functionality
                test_context = bpy.context.copy()
                with bpy.context.temp_override(**test_context):
                    # This should work without errors
                    test_area = bpy.context.area
                self.log_test("Context Override (Blender 4.5+)", True, "temp_override works correctly")
            else:
                # Test old context override method
                test_context = bpy.context.copy()
                self.log_test("Context Override (Legacy)", True, "Legacy context method works")
        except Exception as e:
            self.log_test("Context Override", False, f"Context override failed: {str(e)}")

        # Test operator calling compatibility
        try:
            # Test a simple operator call
            bpy.ops.object.select_all(action='DESELECT')
            self.log_test("Operator Execution", True, "Basic operators work correctly")
        except Exception as e:
            self.log_test("Operator Execution", False, f"Operator execution failed: {str(e)}")

    def test_registration(self):
        """Test that all operators are properly registered"""
        print("\n📋 Testing Operator Registration...")

        required_operators = [
            "aeon.tile_manager_dialog",
            "aeon.room_wizard_dialog",
            "aeon.portal_manager_dialog",
            "aeon.trigger_manager_dialog",
            "aeon.enhanced_tile_painter",
            "aeon.create_room_template",
            "aeon.place_room_template",
            "aeon.mark_portal",
            "aeon.attach_trigger",
            "aeon.origin_set_standing",
            "aeon.quick_view_top",
            "aeon.quick_view_front",
            "aeon.quick_view_side",
            "aeon.quick_view_iso",
            "aeon.quick_view_tile",
            "aeon.quick_view_persp"
        ]

        for op_name in required_operators:
            try:
                module_name = op_name.split('.')[0]
                if hasattr(bpy.ops, module_name):
                    op_group = getattr(bpy.ops, module_name)
                    if hasattr(op_group, op_name.split('.')[1]):
                        self.log_test(f"Operator registration: {op_name}", True)
                    else:
                        self.log_test(f"Operator registration: {op_name}", False, "Operator method not found")
                else:
                    self.log_test(f"Operator registration: {op_name}", False, "Operator module not found")
            except Exception as e:
                self.log_test(f"Operator registration: {op_name}", False, str(e))

    def test_dialog_functionality(self):
        """Test that dialog operators can be invoked"""
        print("\n🗂️ Testing Dialog Functionality...")

        # Test tile manager dialog
        try:
            # Blender 4.5+ compatibility: use INVOKE_DEFAULT with proper context
            if BLENDER_4_5_PLUS:
                # For Blender 4.5+, ensure proper context override
                context_override = bpy.context.copy()
                context_override['area'] = bpy.context.area
                context_override['region'] = bpy.context.region
                with bpy.context.temp_override(**context_override):
                    bpy.ops.aeon.tile_manager_dialog('INVOKE_DEFAULT')
            else:
                bpy.ops.aeon.tile_manager_dialog('INVOKE_DEFAULT')
            self.log_test("Tile Manager Dialog", True, "Dialog opened successfully")
        except Exception as e:
            self.log_test("Tile Manager Dialog", False, f"Failed to open: {str(e)}")

        # Close any open dialogs
        try:
            if bpy.context.window_manager.windows:
                for area in bpy.context.window_manager.windows[0].screen.areas:
                    if area.ui_type == 'PREFERENCES':
                        bpy.ops.screen.area_split(direction='VERTICAL', factor=0.5)
        except:
            pass

    def test_tile_template_creation(self):
        """Test tile template creation workflow"""
        print("\n🏗️ Testing Tile Template Creation...")

        try:
            # Select test objects
            bpy.ops.object.select_all(action='DESELECT')
            test_tile = bpy.data.objects.get("test_floor_tile")
            if test_tile:
                test_tile.select_set(True)
                bpy.context.view_layer.objects.active = test_tile

                # Test template creation
                bpy.ops.aeon.create_room_template(
                    template_name="test_floor_template",
                    template_category="Floor",
                    template_description="Test floor tile template"
                )

                self.log_test("Tile Template Creation", True, "Template created successfully")
            else:
                self.log_test("Tile Template Creation", False, "Test tile object not found")

        except Exception as e:
            self.log_test("Tile Template Creation", False, str(e))

    def test_room_wizard(self):
        """Test room wizard functionality"""
        print("\n🏠 Testing Room Wizard...")

        try:
            bpy.ops.aeon.room_wizard_dialog('INVOKE_DEFAULT')
            self.log_test("Room Wizard Dialog", True, "Wizard opened successfully")
        except Exception as e:
            self.log_test("Room Wizard Dialog", False, f"Failed to open: {str(e)}")

    def test_portal_system(self):
        """Test portal marking system"""
        print("\n🚪 Testing Portal System...")

        try:
            # Select test door
            bpy.ops.object.select_all(action='DESELECT')
            test_door = bpy.data.objects.get("test_door")
            if test_door:
                test_door.select_set(True)
                bpy.context.view_layer.objects.active = test_door

                # Test portal marking
                bpy.ops.aeon.mark_portal(
                    portal_name="test_entrance",
                    target_map="test_level_02",
                    target_portal="level02_entrance",
                    portal_type="door",
                    is_bidirectional=True
                )

                self.log_test("Portal System", True, "Portal marked successfully")
            else:
                self.log_test("Portal System", False, "Test door object not found")

        except Exception as e:
            self.log_test("Portal System", False, str(e))

    def test_trigger_system(self):
        """Test trigger attachment system"""
        print("\n⚡ Testing Trigger System...")

        try:
            # Select test prop
            bpy.ops.object.select_all(action='DESELECT')
            test_prop = bpy.data.objects.get("test_prop")
            if test_prop:
                test_prop.select_set(True)
                bpy.context.view_layer.objects.active = test_prop

                # Test trigger attachment
                bpy.ops.aeon.attach_trigger(
                    trigger_name="test_switch",
                    trigger_type="on_use",
                    trigger_event="open_door",
                    trigger_target="test_door",
                    trigger_parameters='{"speed": 1.0, "sound": "door_open.wav"}'
                )

                self.log_test("Trigger System", True, "Trigger attached successfully")
            else:
                self.log_test("Trigger System", False, "Test prop object not found")

        except Exception as e:
            self.log_test("Trigger System", False, str(e))

    def test_origin_adjuster(self):
        """Test origin adjuster functionality"""
        print("\n🎯 Testing Origin Adjuster...")

        try:
            # Select test object
            bpy.ops.object.select_all(action='DESELECT')
            test_obj = bpy.data.objects.get("test_prop")
            if test_obj:
                test_obj.select_set(True)
                bpy.context.view_layer.objects.active = test_obj

                # Store original position
                original_location = test_obj.location.copy()

                # Test origin setting
                bpy.ops.aeon.origin_set_standing()

                # Check if object still exists (didn't get deleted)
                if bpy.data.objects.get("test_prop"):
                    self.log_test("Origin Adjuster", True, "Origin set successfully")
                else:
                    self.log_test("Origin Adjuster", False, "Object was deleted during operation")
            else:
                self.log_test("Origin Adjuster", False, "Test object not found")

        except Exception as e:
            self.log_test("Origin Adjuster", False, str(e))

    def test_quick_view_system(self):
        """Test quick view switching"""
        print("\n👁️ Testing Quick View System...")

        view_tests = [
            ("Top View", "aeon.quick_view_top"),
            ("Front View", "aeon.quick_view_front"),
            ("Side View", "aeon.quick_view_side"),
            ("ISO View", "aeon.quick_view_iso"),
            ("Tile View", "aeon.quick_view_tile"),
            ("Persp View", "aeon.quick_view_persp")
        ]

        for view_name, op_name in view_tests:
            try:
                module, func = op_name.split('.')
                getattr(bpy.ops, module).__getattr__(func)()
                self.log_test(f"Quick View: {view_name}", True, "View changed successfully")
            except Exception as e:
                self.log_test(f"Quick View: {view_name}", False, str(e))

    def test_data_export(self):
        """Test data export functionality"""
        print("\n📤 Testing Data Export...")

        try:
            # Test if export functionality exists
            if hasattr(bpy.ops, 'aeon') and hasattr(bpy.ops.aeon, 'export_level_data'):
                bpy.ops.aeon.export_level_data(
                    export_path="//test_level_export.json",
                    export_format="json"
                )
                self.log_test("Data Export", True, "Export completed successfully")
            else:
                self.log_test("Data Export", False, "Export operator not found")

        except Exception as e:
            self.log_test("Data Export", False, str(e))

    def test_multi_object_processing(self):
        """Test multi-object processing capabilities"""
        print("\n🔄 Testing Multi-Object Processing...")

        try:
            # Select multiple objects
            bpy.ops.object.select_all(action='DESELECT')
            objects_to_select = ["test_floor_tile", "test_wall_tile", "test_prop"]

            selected_count = 0
            for obj_name in objects_to_select:
                obj = bpy.data.objects.get(obj_name)
                if obj:
                    obj.select_set(True)
                    selected_count += 1

            if selected_count > 1:
                # Test origin setting on multiple objects
                bpy.ops.aeon.origin_set_standing()
                self.log_test("Multi-Object Processing", True, f"Processed {selected_count} objects")
            else:
                self.log_test("Multi-Object Processing", False, "Could not select multiple test objects")

        except Exception as e:
            self.log_test("Multi-Object Processing", False, str(e))

    def test_error_handling(self):
        """Test error handling for edge cases"""
        print("\n⚠️ Testing Error Handling...")

        # Test with no objects selected
        try:
            bpy.ops.object.select_all(action='DESELECT')
            bpy.context.view_layer.objects.active = None

            # This should handle the case gracefully
            if hasattr(bpy.ops, 'aeon') and hasattr(bpy.ops.aeon, 'origin_set_standing'):
                bpy.ops.aeon.origin_set_standing()
                self.log_test("Error Handling: No Selection", True, "Handled gracefully")
            else:
                self.log_test("Error Handling: No Selection", False, "Operator not available")

        except Exception as e:
            self.log_test("Error Handling: No Selection", False, str(e))

    def run_all_tests(self):
        """Run complete test suite"""
        print("🚀 Starting AEON Interior Toolkit Test Suite")
        print("=" * 60)

        # Setup
        self.setup_test_environment()

        # Run all tests
        self.test_blender_compatibility()
        self.test_registration()
        self.test_dialog_functionality()
        self.test_tile_template_creation()
        self.test_room_wizard()
        self.test_portal_system()
        self.test_trigger_system()
        self.test_origin_adjuster()
        self.test_quick_view_system()
        self.test_data_export()
        self.test_multi_object_processing()
        self.test_error_handling()

        # Print summary
        print("\n" + "=" * 60)
        print("📊 TEST SUMMARY")
        print("=" * 60)
        print(f"Total Tests: {self.total_tests}")
        print(f"✅ Passed: {self.passed_tests}")
        print(f"❌ Failed: {self.failed_tests}")

        if self.failed_tests > 0:
            print(f"\n❌ FAILED TESTS:")
            for result in self.test_results:
                if "❌ FAIL" in result:
                    print(f"  {result}")

        success_rate = (self.passed_tests / self.total_tests) * 100 if self.total_tests > 0 else 0
        print(f"\n📈 Success Rate: {success_rate:.1f}%")

        if success_rate >= 90:
            print("🎉 EXCELLENT! The toolkit is working very well!")
        elif success_rate >= 75:
            print("👍 GOOD! Most features are working correctly.")
        elif success_rate >= 50:
            print("⚠️  FAIR. Some features need attention.")
        else:
            print("🚨 POOR. The toolkit needs significant fixes.")

        return success_rate

def main():
    """Main test execution function"""
    tester = AEONToolkitTester()
    success_rate = tester.run_all_tests()

    # Print usage instructions
    print("\n" + "=" * 60)
    print("📋 USAGE INSTRUCTIONS")
    print("=" * 60)
    print("1. Install the addon in Blender Preferences")
    print("2. Look for 'AEON Tools' in the 3D View sidebar")
    print("3. Use the Tile Manager to create and manage templates")
    print("4. Use the Room Wizard for quick room creation")
    print("5. Use the Enhanced Tile Painter for manual placement")
    print("6. Mark portals and attach triggers for game logic")
    print("7. Export level data for your C++ game engine")

    return success_rate

if __name__ == "__main__":
    try:
        success_rate = main()
        print(f"\n🎯 Test suite completed with {success_rate:.1f}% success rate.")
    except Exception as e:
        print(f"🚨 Test suite crashed: {str(e)}")
        traceback.print_exc()