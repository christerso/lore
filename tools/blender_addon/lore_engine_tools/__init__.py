"""
Lore Engine Tools - Professional Blender Addon
==============================================

A comprehensive toolset for creating game-ready assets for the Lore Engine.

Features:
- Material property editor (structural, thermal, chemical)
- Advanced fracture generation (Voronoi, procedural cracks)
- Pre-compute fracture variations
- Material preset library
- glTF export with metadata validation

Version: 1.0.0
Author: Lore Engine Development Team
License: Commercial (Sellable)
"""

bl_info = {
    "name": "Lore Engine Tools",
    "author": "Lore Engine Development Team",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "View3D > Sidebar > Lore Engine",
    "description": "Professional toolset for Lore Engine game asset creation",
    "warning": "",
    "doc_url": "https://github.com/lore-engine/docs",
    "category": "Game Engine",
}

import bpy

# Import modules
from . import properties
from . import operators
from . import panels
from . import presets
from . import fracture
from . import export_tools

# Module list for registration
modules = [
    properties,
    operators,
    panels,
    presets,
    fracture,
    export_tools,
]

def register():
    """Register all addon classes and properties"""
    # Register all modules
    for module in modules:
        if hasattr(module, 'register'):
            module.register()

    print("Lore Engine Tools v1.0.0 loaded successfully")

def unregister():
    """Unregister all addon classes and properties"""
    # Unregister in reverse order
    for module in reversed(modules):
        if hasattr(module, 'unregister'):
            module.unregister()

    print("Lore Engine Tools v1.0.0 unloaded")

if __name__ == "__main__":
    register()