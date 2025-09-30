"""
General Operators
=================

Utility operators for the Lore Engine Tools addon.
"""

import bpy
from bpy.types import Operator

class LORE_OT_ValidateProperties(Operator):
    """Validate all Lore Engine properties for export"""
    bl_idname = "lore.validate_properties"
    bl_label = "Validate Properties"
    bl_description = "Check all Lore properties for errors and missing values"

    def execute(self, context):
        errors = []
        warnings = []

        # Validate all materials
        for mat in bpy.data.materials:
            if mat.users == 0:
                continue

            # Check structural properties
            struct = mat.lore_structural
            if struct.density <= 0:
                errors.append(f"Material '{mat.name}': Density must be > 0")
            if struct.youngs_modulus <= 0:
                errors.append(f"Material '{mat.name}': Young's Modulus must be > 0")

            # Check thermal properties
            thermal = mat.lore_thermal
            if thermal.melting_point > 0 and thermal.melting_point < thermal.initial_temperature:
                warnings.append(f"Material '{mat.name}': Initial temp higher than melting point")

            # Check chemical properties
            chem = mat.lore_chemical
            if chem.is_combustible and chem.ignition_temperature == 0:
                warnings.append(f"Material '{mat.name}': Combustible but no ignition temperature set")

        # Validate all objects
        for obj in context.scene.objects:
            if obj.type != 'MESH':
                continue

            # Check fracture properties
            fracture = obj.lore_fracture
            if fracture.fracture_enabled and fracture.voronoi_cell_count < 3:
                errors.append(f"Object '{obj.name}': Voronoi cell count must be >= 3")

        # Report results
        if errors:
            for error in errors:
                self.report({'ERROR'}, error)
            self.report({'ERROR'}, f"Validation failed with {len(errors)} errors")
            return {'CANCELLED'}

        if warnings:
            for warning in warnings:
                self.report({'WARNING'}, warning)

        self.report({'INFO'}, "Validation passed!" if not warnings else f"Validation passed with {len(warnings)} warnings")
        return {'FINISHED'}


classes = [
    LORE_OT_ValidateProperties,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)