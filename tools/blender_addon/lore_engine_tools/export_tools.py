"""
glTF Export with Metadata
==========================

Export Blender scenes to glTF 2.0 format with Lore Engine metadata
embedded in the 'extras' field.
"""

import bpy
import json
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper

class LORE_OT_ExportGLTF(Operator, ExportHelper):
    """Export scene to glTF with Lore Engine metadata"""
    bl_idname = "lore.export_gltf"
    bl_label = "Export Lore glTF"
    bl_description = "Export to glTF 2.0 with Lore Engine metadata"

    filename_ext = ".gltf"
    filter_glob: bpy.props.StringProperty(default="*.gltf;*.glb", options={'HIDDEN'})

    export_format: bpy.props.EnumProperty(
        name="Format",
        items=[
            ('GLTF_SEPARATE', "glTF Separate", "Export as .gltf with separate .bin"),
            ('GLTF_EMBEDDED', "glTF Embedded", "Export as .gltf with embedded data"),
            ('GLB', "glTF Binary", "Export as single .glb file"),
        ],
        default='GLB'
    )

    def execute(self, context):
        # Prepare metadata for export
        self.prepare_metadata(context)

        # Use Blender's built-in glTF exporter with custom properties
        bpy.ops.export_scene.gltf(
            filepath=self.filepath,
            export_format=self.export_format,
            export_extras=True,  # Include custom properties
            export_yup=True,
        )

        self.report({'INFO'}, f"Exported to {self.filepath}")
        return {'FINISHED'}

    def prepare_metadata(self, context):
        """Prepare all Lore Engine metadata for export"""

        # Process all materials
        for mat in bpy.data.materials:
            if mat.users == 0:
                continue

            # Create extras dict if not exists
            if not hasattr(mat, 'extras'):
                mat['extras'] = {}

            extras = {}

            # Structural properties
            struct = mat.lore_structural
            extras['lore_structural_material'] = {
                'youngs_modulus': struct.youngs_modulus,
                'poisson_ratio': struct.poisson_ratio,
                'density': struct.density,
                'fracture_toughness': struct.fracture_toughness,
                'yield_strength': struct.yield_strength,
                'ultimate_strength': struct.ultimate_strength,
                'hardness': struct.hardness,
                'ductility': struct.ductility,
                'damping_coefficient': struct.damping_coefficient,
            }

            # Thermal properties
            thermal = mat.lore_thermal
            extras['lore_thermal_properties'] = {
                'specific_heat_capacity': thermal.specific_heat_capacity,
                'thermal_conductivity': thermal.thermal_conductivity,
                'thermal_expansion_coeff': thermal.thermal_expansion_coeff,
                'melting_point': thermal.melting_point,
                'boiling_point': thermal.boiling_point,
                'ignition_temperature': thermal.ignition_temperature,
                'emissivity': thermal.emissivity,
                'initial_temperature': thermal.initial_temperature,
            }

            # Chemical properties
            chem = mat.lore_chemical
            extras['lore_chemical_composition'] = {
                'chemical_formula': chem.chemical_formula,
                'is_combustible': chem.is_combustible,
                'oxidation_rate': chem.oxidation_rate,
                'heat_of_combustion': chem.heat_of_combustion,
                'oxygen_required': chem.oxygen_required,
                'combustion_efficiency': chem.combustion_efficiency,
                'soot_production_rate': chem.soot_production_rate,
                'ash_residue_fraction': chem.ash_residue_fraction,
            }

            # Store in material
            mat['lore_engine_metadata'] = json.dumps(extras)

        # Process all objects
        for obj in context.scene.objects:
            if obj.type != 'MESH':
                continue

            extras = {}

            # Fracture properties
            fracture = obj.lore_fracture
            if fracture.fracture_enabled:
                extras['lore_fracture_properties'] = {
                    'fracture_enabled': fracture.fracture_enabled,
                    'voronoi_cell_count': fracture.voronoi_cell_count,
                    'fracture_seed': fracture.fracture_seed,
                    'fracture_randomness': fracture.fracture_randomness,
                    'fracture_pattern': fracture.fracture_pattern,
                    'minimum_piece_volume': fracture.minimum_piece_volume,
                    'damage_propagation_speed': fracture.damage_propagation_speed,
                    'impact_threshold_energy': fracture.impact_threshold_energy,
                    'shatter_completely': fracture.shatter_completely,
                }

            # Volumetric fire properties
            fire = obj.lore_volumetric_fire
            extras['lore_volumetric_fire'] = {
                'grid_resolution_x': fire.grid_resolution_x,
                'grid_resolution_y': fire.grid_resolution_y,
                'grid_resolution_z': fire.grid_resolution_z,
                'cell_size': fire.cell_size,
                'source_temperature': fire.source_temperature,
                'source_fuel_rate': fire.source_fuel_rate,
                'buoyancy_coefficient': fire.buoyancy_coefficient,
                'vorticity_strength': fire.vorticity_strength,
                'emission_strength': fire.emission_strength,
            }

            # Volumetric fluid properties
            fluid = obj.lore_volumetric_fluid
            extras['lore_volumetric_fluid'] = {
                'fluid_type': fluid.fluid_type,
                'grid_resolution_x': fluid.grid_resolution_x,
                'grid_resolution_y': fluid.grid_resolution_y,
                'grid_resolution_z': fluid.grid_resolution_z,
                'cell_size': fluid.cell_size,
                'fluid_density': fluid.fluid_density,
                'dynamic_viscosity': fluid.dynamic_viscosity,
                'surface_tension': fluid.surface_tension,
                'refraction_index': fluid.refraction_index,
                'enable_foam': fluid.enable_foam,
                'enable_splashes': fluid.enable_splashes,
                'enable_caustics': fluid.enable_caustics,
            }

            # Store in object
            obj['lore_engine_metadata'] = json.dumps(extras)


classes = [
    LORE_OT_ExportGLTF,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    # Add to File > Export menu
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

def menu_func_export(self, context):
    self.layout.operator(LORE_OT_ExportGLTF.bl_idname, text="Lore Engine glTF (.gltf/.glb)")