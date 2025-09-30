"""
UI Panels
=========

Professional UI panels for the Lore Engine Tools addon.
Displayed in 3D Viewport sidebar under "Lore Engine" tab.
"""

import bpy
from bpy.types import Panel

# =============================================================================
# Main Panel
# =============================================================================

class LORE_PT_MainPanel(Panel):
    """Main panel in 3D Viewport sidebar"""
    bl_label = "Lore Engine Tools"
    bl_idname = "LORE_PT_main_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'

    def draw(self, context):
        layout = self.layout
        layout.label(text="Lore Engine Asset Tools", icon='PHYSICS')
        layout.operator("lore.validate_properties", icon='CHECKMARK')

# =============================================================================
# Material Property Panels
# =============================================================================

class LORE_PT_MaterialPresets(Panel):
    """Material preset library panel"""
    bl_label = "Material Presets"
    bl_idname = "LORE_PT_material_presets"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        if not context.active_object or not context.active_object.active_material:
            layout.label(text="No active material", icon='ERROR')
            return

        layout.label(text="Quick Apply:")
        layout.operator("lore.apply_material_preset", text="Steel").preset_name = 'STEEL'
        layout.operator("lore.apply_material_preset", text="Wood").preset_name = 'WOOD'
        layout.operator("lore.apply_material_preset", text="Concrete").preset_name = 'CONCRETE'
        layout.operator("lore.apply_material_preset", text="Glass").preset_name = 'GLASS'
        layout.operator("lore.apply_material_preset", text="Aluminum").preset_name = 'ALUMINUM'
        layout.operator("lore.apply_material_preset", text="Plastic").preset_name = 'PLASTIC'


class LORE_PT_StructuralProperties(Panel):
    """Structural material properties panel"""
    bl_label = "Structural Properties"
    bl_idname = "LORE_PT_structural_properties"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.active_material

    def draw(self, context):
        layout = self.layout
        mat = context.active_object.active_material
        props = mat.lore_structural

        layout.label(text="Mechanical Properties:")
        layout.prop(props, "youngs_modulus")
        layout.prop(props, "poisson_ratio")
        layout.prop(props, "density")

        layout.separator()
        layout.label(text="Strength:")
        layout.prop(props, "yield_strength")
        layout.prop(props, "ultimate_strength")
        layout.prop(props, "fracture_toughness")

        layout.separator()
        layout.label(text="Characteristics:")
        layout.prop(props, "hardness")
        layout.prop(props, "ductility")
        layout.prop(props, "damping_coefficient")


class LORE_PT_ThermalProperties(Panel):
    """Thermal properties panel"""
    bl_label = "Thermal Properties"
    bl_idname = "LORE_PT_thermal_properties"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.active_material

    def draw(self, context):
        layout = self.layout
        mat = context.active_object.active_material
        props = mat.lore_thermal

        layout.label(text="Heat Transfer:")
        layout.prop(props, "specific_heat_capacity")
        layout.prop(props, "thermal_conductivity")
        layout.prop(props, "thermal_expansion_coeff")

        layout.separator()
        layout.label(text="Phase Transitions:")
        layout.prop(props, "melting_point")
        layout.prop(props, "boiling_point")
        layout.prop(props, "ignition_temperature")

        layout.separator()
        layout.prop(props, "emissivity")
        layout.prop(props, "initial_temperature")


class LORE_PT_ChemicalProperties(Panel):
    """Chemical composition panel"""
    bl_label = "Chemical Properties"
    bl_idname = "LORE_PT_chemical_properties"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.active_material

    def draw(self, context):
        layout = self.layout
        mat = context.active_object.active_material
        props = mat.lore_chemical

        layout.prop(props, "chemical_formula")
        layout.prop(props, "is_combustible")

        if props.is_combustible:
            layout.separator()
            layout.label(text="Combustion Properties:")
            layout.prop(props, "oxidation_rate")
            layout.prop(props, "heat_of_combustion")
            layout.prop(props, "oxygen_required")
            layout.prop(props, "combustion_efficiency")
            layout.prop(props, "soot_production_rate")
            layout.prop(props, "ash_residue_fraction")

# =============================================================================
# Fracture Panels
# =============================================================================

class LORE_PT_FractureTools(Panel):
    """Fracture generation tools panel"""
    bl_label = "Fracture Tools"
    bl_idname = "LORE_PT_fracture_tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH'

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        props = obj.lore_fracture

        layout.prop(props, "fracture_enabled")

        if not props.fracture_enabled:
            return

        layout.separator()
        layout.label(text="Voronoi Fracture:")
        layout.prop(props, "voronoi_cell_count")
        layout.prop(props, "fracture_pattern")
        layout.prop(props, "fracture_randomness")
        layout.prop(props, "fracture_seed")

        layout.separator()
        layout.operator("lore.voronoi_fracture", icon='MOD_EXPLODE')

        layout.separator()
        layout.label(text="Surface Details:")
        layout.operator("lore.add_cracks", icon='OUTLINER_DATA_LIGHTPROBE')

        layout.separator()
        layout.label(text="Batch Processing:")
        layout.operator("lore.batch_fracture", icon='DUPLICATE')

        layout.separator()
        layout.label(text="Advanced:")
        layout.prop(props, "minimum_piece_volume")
        layout.prop(props, "damage_propagation_speed")
        layout.prop(props, "impact_threshold_energy")
        layout.prop(props, "shatter_completely")


class LORE_PT_VolumetricFire(Panel):
    """Volumetric fire properties panel"""
    bl_label = "Volumetric Fire"
    bl_idname = "LORE_PT_volumetric_fire"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        props = obj.lore_volumetric_fire

        layout.label(text="Grid Resolution:")
        row = layout.row(align=True)
        row.prop(props, "grid_resolution_x", text="X")
        row.prop(props, "grid_resolution_y", text="Y")
        row.prop(props, "grid_resolution_z", text="Z")

        layout.prop(props, "cell_size")

        layout.separator()
        layout.label(text="Fire Source:")
        layout.prop(props, "source_temperature")
        layout.prop(props, "source_fuel_rate")

        layout.separator()
        layout.label(text="Simulation:")
        layout.prop(props, "buoyancy_coefficient")
        layout.prop(props, "vorticity_strength")

        layout.separator()
        layout.label(text="Rendering:")
        layout.prop(props, "emission_strength")


class LORE_PT_VolumetricFluid(Panel):
    """Volumetric fluid properties panel"""
    bl_label = "Volumetric Fluid"
    bl_idname = "LORE_PT_volumetric_fluid"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Lore Engine'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        props = obj.lore_volumetric_fluid

        layout.prop(props, "fluid_type")

        layout.separator()
        layout.label(text="Grid Resolution:")
        row = layout.row(align=True)
        row.prop(props, "grid_resolution_x", text="X")
        row.prop(props, "grid_resolution_y", text="Y")
        row.prop(props, "grid_resolution_z", text="Z")

        layout.prop(props, "cell_size")

        layout.separator()
        layout.label(text="Physical Properties:")
        layout.prop(props, "fluid_density")
        layout.prop(props, "dynamic_viscosity")
        layout.prop(props, "surface_tension")
        layout.prop(props, "refraction_index")

        layout.separator()
        layout.label(text="Effects:")
        layout.prop(props, "enable_foam")
        layout.prop(props, "enable_splashes")
        layout.prop(props, "enable_caustics")

# =============================================================================
# Registration
# =============================================================================

classes = [
    LORE_PT_MainPanel,
    LORE_PT_MaterialPresets,
    LORE_PT_StructuralProperties,
    LORE_PT_ThermalProperties,
    LORE_PT_ChemicalProperties,
    LORE_PT_FractureTools,
    LORE_PT_VolumetricFire,
    LORE_PT_VolumetricFluid,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)