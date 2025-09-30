"""
Material Preset Library
========================

Predefined material properties for common materials used in game development.
All values are physically accurate and based on real-world measurements.
"""

import bpy
from bpy.types import Operator

# Material preset data structure
MATERIAL_PRESETS = {
    'STEEL': {
        'name': "Steel (Structural Metal)",
        'structural': {
            'youngs_modulus': 200000.0,
            'poisson_ratio': 0.3,
            'density': 7850.0,
            'fracture_toughness': 50.0,
            'yield_strength': 250.0,
            'ultimate_strength': 400.0,
            'hardness': 200.0,
            'ductility': 0.8,
            'damping_coefficient': 0.01,
        },
        'thermal': {
            'specific_heat_capacity': 500.0,
            'thermal_conductivity': 50.0,
            'thermal_expansion_coeff': 0.000012,
            'melting_point': 1811.0,
            'boiling_point': 3134.0,
            'ignition_temperature': 0.0,
            'emissivity': 0.7,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "Fe_C",
            'is_combustible': False,
        },
        'fracture': {
            'voronoi_cell_count': 20,
            'fracture_pattern': 'RANDOM',
            'impact_threshold_energy': 500.0,
        }
    },

    'WOOD': {
        'name': "Wood (Cellulose)",
        'structural': {
            'youngs_modulus': 11000.0,
            'poisson_ratio': 0.3,
            'density': 600.0,
            'fracture_toughness': 5.0,
            'yield_strength': 50.0,
            'ultimate_strength': 100.0,
            'hardness': 30.0,
            'ductility': 0.3,
            'damping_coefficient': 0.05,
        },
        'thermal': {
            'specific_heat_capacity': 1700.0,
            'thermal_conductivity': 0.15,
            'thermal_expansion_coeff': 0.000005,
            'melting_point': 0.0,
            'boiling_point': 0.0,
            'ignition_temperature': 573.0,
            'emissivity': 0.9,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "C6H10O5",
            'is_combustible': True,
            'oxidation_rate': 3.0,
            'heat_of_combustion': 2800.0,
            'oxygen_required': 6.0,
            'combustion_efficiency': 0.95,
            'soot_production_rate': 0.15,
            'ash_residue_fraction': 0.05,
        },
        'fracture': {
            'voronoi_cell_count': 15,
            'fracture_pattern': 'SPLINTER',
            'impact_threshold_energy': 50.0,
        }
    },

    'CONCRETE': {
        'name': "Concrete (Structural)",
        'structural': {
            'youngs_modulus': 30000.0,
            'poisson_ratio': 0.2,
            'density': 2400.0,
            'fracture_toughness': 1.5,
            'yield_strength': 40.0,
            'ultimate_strength': 50.0,
            'hardness': 100.0,
            'ductility': 0.1,
            'damping_coefficient': 0.02,
        },
        'thermal': {
            'specific_heat_capacity': 880.0,
            'thermal_conductivity': 1.4,
            'thermal_expansion_coeff': 0.000012,
            'melting_point': 1923.0,
            'boiling_point': 3000.0,
            'ignition_temperature': 0.0,
            'emissivity': 0.95,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "CaCO3_SiO2",
            'is_combustible': False,
        },
        'fracture': {
            'voronoi_cell_count': 30,
            'fracture_pattern': 'GRID',
            'impact_threshold_energy': 200.0,
        }
    },

    'GLASS': {
        'name': "Glass (Transparent)",
        'structural': {
            'youngs_modulus': 70000.0,
            'poisson_ratio': 0.24,
            'density': 2500.0,
            'fracture_toughness': 0.7,
            'yield_strength': 50.0,
            'ultimate_strength': 50.0,
            'hardness': 600.0,
            'ductility': 0.0,
            'damping_coefficient': 0.001,
        },
        'thermal': {
            'specific_heat_capacity': 840.0,
            'thermal_conductivity': 1.0,
            'thermal_expansion_coeff': 0.000009,
            'melting_point': 1873.0,
            'boiling_point': 2500.0,
            'ignition_temperature': 0.0,
            'emissivity': 0.9,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "SiO2",
            'is_combustible': False,
        },
        'fracture': {
            'voronoi_cell_count': 50,
            'fracture_pattern': 'RADIAL',
            'impact_threshold_energy': 20.0,
            'shatter_completely': True,
        }
    },

    'ALUMINUM': {
        'name': "Aluminum (Light Metal)",
        'structural': {
            'youngs_modulus': 69000.0,
            'poisson_ratio': 0.33,
            'density': 2700.0,
            'fracture_toughness': 30.0,
            'yield_strength': 95.0,
            'ultimate_strength': 110.0,
            'hardness': 150.0,
            'ductility': 0.9,
            'damping_coefficient': 0.01,
        },
        'thermal': {
            'specific_heat_capacity': 897.0,
            'thermal_conductivity': 237.0,
            'thermal_expansion_coeff': 0.000023,
            'melting_point': 933.0,
            'boiling_point': 2792.0,
            'ignition_temperature': 0.0,
            'emissivity': 0.2,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "Al",
            'is_combustible': False,
        },
        'fracture': {
            'voronoi_cell_count': 15,
            'fracture_pattern': 'RANDOM',
            'impact_threshold_energy': 300.0,
        }
    },

    'PLASTIC': {
        'name': "Plastic (ABS)",
        'structural': {
            'youngs_modulus': 2300.0,
            'poisson_ratio': 0.35,
            'density': 1040.0,
            'fracture_toughness': 4.0,
            'yield_strength': 40.0,
            'ultimate_strength': 45.0,
            'hardness': 80.0,
            'ductility': 0.6,
            'damping_coefficient': 0.1,
        },
        'thermal': {
            'specific_heat_capacity': 1400.0,
            'thermal_conductivity': 0.17,
            'thermal_expansion_coeff': 0.00009,
            'melting_point': 378.0,
            'boiling_point': 0.0,
            'ignition_temperature': 673.0,
            'emissivity': 0.95,
            'initial_temperature': 293.15,
        },
        'chemical': {
            'chemical_formula': "C8H8",
            'is_combustible': True,
            'oxidation_rate': 5.0,
            'heat_of_combustion': 4000.0,
            'oxygen_required': 10.0,
            'combustion_efficiency': 0.85,
            'soot_production_rate': 0.4,
            'ash_residue_fraction': 0.0,
        },
        'fracture': {
            'voronoi_cell_count': 10,
            'fracture_pattern': 'RANDOM',
            'impact_threshold_energy': 30.0,
        }
    },
}


class LORE_OT_ApplyMaterialPreset(Operator):
    """Apply material preset to active material"""
    bl_idname = "lore.apply_material_preset"
    bl_label = "Apply Material Preset"
    bl_options = {'REGISTER', 'UNDO'}

    preset_name: bpy.props.EnumProperty(
        name="Material Preset",
        description="Select material preset to apply",
        items=[
            ('STEEL', "Steel", "Structural steel (Fe_C)"),
            ('WOOD', "Wood", "Cellulose wood (C6H10O5)"),
            ('CONCRETE', "Concrete", "Building concrete (CaCO3_SiO2)"),
            ('GLASS', "Glass", "Transparent glass (SiO2)"),
            ('ALUMINUM', "Aluminum", "Lightweight aluminum (Al)"),
            ('PLASTIC', "Plastic", "ABS plastic (C8H8)"),
        ]
    )

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.active_material

    def execute(self, context):
        mat = context.active_object.active_material
        preset = MATERIAL_PRESETS.get(self.preset_name)

        if not preset:
            self.report({'ERROR'}, f"Preset {self.preset_name} not found")
            return {'CANCELLED'}

        # Apply structural properties
        if 'structural' in preset:
            for key, value in preset['structural'].items():
                setattr(mat.lore_structural, key, value)

        # Apply thermal properties
        if 'thermal' in preset:
            for key, value in preset['thermal'].items():
                setattr(mat.lore_thermal, key, value)

        # Apply chemical properties
        if 'chemical' in preset:
            for key, value in preset['chemical'].items():
                setattr(mat.lore_chemical, key, value)

        # Apply fracture properties to object
        obj = context.active_object
        if 'fracture' in preset:
            for key, value in preset['fracture'].items():
                if hasattr(obj.lore_fracture, key):
                    setattr(obj.lore_fracture, key, value)

        self.report({'INFO'}, f"Applied {preset['name']} preset")
        return {'FINISHED'}


classes = [
    LORE_OT_ApplyMaterialPreset,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)