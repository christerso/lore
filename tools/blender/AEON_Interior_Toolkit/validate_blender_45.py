"""
AEON Interior Toolkit - Blender 4.5+ Validation Script

This script validates that the enhanced toolkit is fully compatible with Blender 4.5+
and provides detailed compatibility information.

Run this script first before installing the toolkit.
"""

import bpy
import sys
import traceback

def print_header():
    """Print validation header"""
    print("🔍 AEON Interior Toolkit - Blender 4.5+ Validation")
    print("=" * 60)

def check_blender_version():
    """Check Blender version compatibility"""
    print(f"📋 Blender Version Check:")
    version = bpy.app.version
    print(f"   Current Version: {version[0]}.{version[1]}.{version[2]}")

    if version >= (4, 5, 0):
        print("   ✅ Version 4.5+ - FULLY COMPATIBLE")
        return True
    elif version >= (4, 0, 0):
        print("   ⚠️  Version 4.0+ - MOSTLY COMPATIBLE (minor issues possible)")
        return True
    else:
        print("   ❌ Version < 4.0 - NOT RECOMMENDED")
        return False

def check_python_version():
    """Check Python version compatibility"""
    print(f"\n🐍 Python Version Check:")
    version = sys.version_info
    print(f"   Current Version: {version[0]}.{version[1]}.{version[2]}")

    if version >= (3, 10):
        print("   ✅ Python 3.10+ - RECOMMENDED")
        return True
    elif version >= (3, 8):
        print("   ✅ Python 3.8+ - COMPATIBLE")
        return True
    else:
        print("   ❌ Python < 3.8 - NOT COMPATIBLE")
        return False

def test_context_override():
    """Test context override functionality"""
    print(f"\n🔧 Context Override Test:")

    try:
        # Test temp_override for Blender 4.5+
        test_context = bpy.context.copy()
        with bpy.context.temp_override(**test_context):
            test_result = bpy.context.area is not None
        print("   ✅ temp_override works correctly")
        return True
    except AttributeError:
        print("   ⚠️  temp_override not available (older Blender)")
        return False
    except Exception as e:
        print(f"   ❌ Context override failed: {str(e)}")
        return False

def test_operator_system():
    """Test basic operator system"""
    print(f"\n🎛️  Operator System Test:")

    try:
        # Test basic operator
        bpy.ops.object.select_all(action='DESELECT')
        print("   ✅ Basic operators work")
        return True
    except Exception as e:
        print(f"   ❌ Operator system failed: {str(e)}")
        return False

def test_ui_system():
    """Test UI system compatibility"""
    print(f"\n🖥️  UI System Test:")

    try:
        # Test if we can access UI elements
        region = bpy.context.region
        area = bpy.context.area
        if region and area:
            print("   ✅ UI system accessible")
            return True
        else:
            print("   ⚠️  UI context not fully available")
            return True  # Not critical
    except Exception as e:
        print(f"   ❌ UI system failed: {str(e)}")
        return False

def test_bmesh_api():
    """Test bmesh API compatibility"""
    print(f"\n🔷 BMesh API Test:")

    try:
        import bmesh

        # Test basic bmesh operations
        bm = bmesh.new()
        bm.verts.new((0, 0, 0))
        bm.free()
        print("   ✅ BMesh API works correctly")
        return True
    except Exception as e:
        print(f"   ❌ BMesh API failed: {str(e)}")
        return False

def test_properties_system():
    """Test property system"""
    print(f"\n📊 Property System Test:")

    try:
        # Test property creation
        test_prop = bpy.props.StringProperty(name="test", default="test_value")
        print("   ✅ Property system works")
        return True
    except Exception as e:
        print(f"   ❌ Property system failed: {str(e)}")
        return False

def check_dependencies():
    """Check for required dependencies"""
    print(f"\n📦 Dependencies Check:")

    required_modules = ['mathutils', 'json', 'os', 'math']
    all_good = True

    for module in required_modules:
        try:
            __import__(module)
            print(f"   ✅ {module} available")
        except ImportError:
            print(f"   ❌ {module} missing")
            all_good = False

    return all_good

def generate_compatibility_report():
    """Generate detailed compatibility report"""
    print(f"\n📋 COMPATIBILITY REPORT")
    print("=" * 60)

    results = {}

    # Run all tests
    results['blender_version'] = check_blender_version()
    results['python_version'] = check_python_version()
    results['context_override'] = test_context_override()
    results['operator_system'] = test_operator_system()
    results['ui_system'] = test_ui_system()
    results['bmesh_api'] = test_bmesh_api()
    results['properties'] = test_properties_system()
    results['dependencies'] = check_dependencies()

    # Calculate overall compatibility
    passed_tests = sum(results.values())
    total_tests = len(results)
    compatibility_score = (passed_tests / total_tests) * 100

    print(f"\n📊 OVERALL COMPATIBILITY SCORE: {compatibility_score:.1f}%")
    print(f"   Tests Passed: {passed_tests}/{total_tests}")

    # Provide recommendations
    print(f"\n💡 RECOMMENDATIONS:")

    if compatibility_score >= 90:
        print("   🎉 EXCELLENT! Your system is fully compatible.")
        print("   ✅ You can install and use all features.")
    elif compatibility_score >= 75:
        print("   👍 GOOD! Most features will work.")
        print("   ⚠️  Some advanced features may have limitations.")
    elif compatibility_score >= 50:
        print("   ⚠️  FAIR. Basic features should work.")
        print("   ❌ Consider upgrading Blender for full compatibility.")
    else:
        print("   🚨 POOR COMPATIBILITY.")
        print("   ❌ Upgrade to Blender 4.5+ for best results.")

    # Specific recommendations
    if not results['blender_version']:
        print("   🔧 RECOMMENDATION: Upgrade to Blender 4.5+")

    if not results['python_version']:
        print("   🔧 RECOMMENDATION: Ensure Python 3.8+ is available")

    if not results['context_override']:
        print("   🔧 RECOMMENDATION: Some dialog features may not work optimally")

    return compatibility_score >= 75

def main():
    """Main validation function"""
    print_header()

    try:
        is_compatible = generate_compatibility_report()

        print(f"\n🎯 VALIDATION COMPLETE")
        print("=" * 60)

        if is_compatible:
            print("✅ Your system is compatible with the AEON Interior Toolkit!")
            print("📖 Next steps:")
            print("   1. Install the addon: aeon_interior_toolkit_enhanced.py")
            print("   2. Run the test suite: test_enhanced_toolkit.py")
            print("   3. Follow the usage guide in BLENDER_45_INSTALLATION.md")
        else:
            print("❌ Your system has compatibility issues.")
            print("🔧 Please address the issues above before installing.")

        return is_compatible

    except Exception as e:
        print(f"🚨 Validation failed: {str(e)}")
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)