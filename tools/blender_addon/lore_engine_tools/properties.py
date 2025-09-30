"""
Lore Engine Custom Properties
==============================

Defines all custom properties for materials and objects that map to
Lore Engine's physics, thermal, chemical, and structural systems.
"""

import bpy
from bpy.props import (
    FloatProperty,
    IntProperty,
    BoolProperty,
    StringProperty,
    EnumProperty,
    PointerProperty,
    CollectionProperty,
)
from bpy.types import PropertyGroup

# =============================================================================
# Structural Material Properties
# =============================================================================

class LoreStructuralMaterialProperties(PropertyGroup):
    """Structural material properties for physics simulation"""

    youngs_modulus: FloatProperty(
        name="Young's Modulus",
        description="Material stiffness (resistance to elastic deformation) in MPa",
        default=200000.0,
        min=0.1,
        max=500000.0,
        subtype='NONE',
        unit='NONE',
    )

    poisson_ratio: FloatProperty(
        name="Poisson's Ratio",
        description="Lateral strain vs axial strain ratio (dimensionless)",
        default=0.3,
        min=0.0,
        max=0.5,
    )

    density: FloatProperty(
        name="Density",
        description="Mass per unit volume in kg/m³",
        default=7850.0,
        min=10.0,
        max=20000.0,
        unit='NONE',
    )

    fracture_toughness: FloatProperty(
        name="Fracture Toughness",
        description="Resistance to crack propagation in MPa√m",
        default=50.0,
        min=0.01,
        max=200.0,
    )

    yield_strength: FloatProperty(
        name="Yield Strength",
        description="Stress at which plastic deformation begins in MPa",
        default=250.0,
        min=0.1,
        max=5000.0,
    )

    ultimate_strength: FloatProperty(
        name="Ultimate Strength",
        description="Maximum stress before failure in MPa",
        default=400.0,
        min=1.0,
        max=6000.0,
    )

    hardness: FloatProperty(
        name="Hardness",
        description="Vickers hardness (resistance to indentation) in HV",
        default=200.0,
        min=10.0,
        max=10000.0,
    )

    ductility: FloatProperty(
        name="Ductility",
        description="Ability to deform plastically (0=brittle, 1=ductile)",
        default=0.8,
        min=0.0,
        max=1.0,
    )

    damping_coefficient: FloatProperty(
        name="Damping Coefficient",
        description="Energy dissipation during vibration (0-1)",
        default=0.01,
        min=0.0,
        max=1.0,
    )

# =============================================================================
# Fracture Properties
# =============================================================================

class LoreFractureProperties(PropertyGroup):
    """Fracture and destruction properties"""

    fracture_enabled: BoolProperty(
        name="Enable Fracture",
        description="Can this object fracture and break apart?",
        default=True,
    )

    voronoi_cell_count: IntProperty(
        name="Voronoi Cells",
        description="Number of fracture pieces to generate",
        default=25,
        min=3,
        max=500,
    )

    fracture_seed: IntProperty(
        name="Random Seed",
        description="Seed for reproducible fracture patterns",
        default=12345,
        min=0,
        max=99999,
    )

    fracture_randomness: FloatProperty(
        name="Randomness",
        description="Cell distribution randomness (0=uniform, 1=chaotic)",
        default=0.8,
        min=0.0,
        max=1.0,
    )

    fracture_pattern: EnumProperty(
        name="Fracture Pattern",
        description="Type of fracture pattern to generate",
        items=[
            ('RANDOM', "Random", "Irregular chunks (rock, ceramics)"),
            ('RADIAL', "Radial", "Cracks radiate from impact (glass, ice)"),
            ('GRID', "Grid", "Rectangular chunks (concrete, masonry)"),
            ('SPLINTER', "Splinter", "Long shards (wood, bone)"),
        ],
        default='RANDOM',
    )

    minimum_piece_volume: FloatProperty(
        name="Minimum Piece Volume",
        description="Smallest allowed fragment size in m³",
        default=0.01,
        min=0.0001,
        max=1.0,
    )

    damage_propagation_speed: FloatProperty(
        name="Crack Speed",
        description="How fast cracks spread through material in m/s",
        default=10.0,
        min=0.1,
        max=100.0,
    )

    impact_threshold_energy: FloatProperty(
        name="Impact Threshold",
        description="Energy needed to start fracture in Joules",
        default=100.0,
        min=0.1,
        max=10000.0,
    )

    shatter_completely: BoolProperty(
        name="Shatter Completely",
        description="Break into all pieces at once (vs progressive fracture)",
        default=False,
    )

# =============================================================================
# Thermal Properties
# =============================================================================

class LoreThermalProperties(PropertyGroup):
    """Thermal and heat transfer properties"""

    specific_heat_capacity: FloatProperty(
        name="Specific Heat Capacity",
        description="Energy to raise 1kg by 1K in J/(kg·K)",
        default=500.0,
        min=100.0,
        max=5000.0,
    )

    thermal_conductivity: FloatProperty(
        name="Thermal Conductivity",
        description="Rate of heat flow through material in W/(m·K)",
        default=50.0,
        min=0.01,
        max=500.0,
    )

    thermal_expansion_coeff: FloatProperty(
        name="Thermal Expansion",
        description="Expansion per degree Kelvin (1/K)",
        default=0.000012,
        min=0.0,
        max=0.0001,
        precision=8,
    )

    melting_point: FloatProperty(
        name="Melting Point",
        description="Temperature at which solid → liquid (Kelvin)",
        default=1811.0,
        min=200.0,
        max=4000.0,
    )

    boiling_point: FloatProperty(
        name="Boiling Point",
        description="Temperature at which liquid → gas (Kelvin)",
        default=3134.0,
        min=300.0,
        max=6000.0,
    )

    ignition_temperature: FloatProperty(
        name="Ignition Temperature",
        description="Temperature at which material ignites (0 = non-flammable)",
        default=0.0,
        min=0.0,
        max=2000.0,
    )

    emissivity: FloatProperty(
        name="Emissivity",
        description="How efficiently material radiates heat (0=mirror, 1=blackbody)",
        default=0.7,
        min=0.0,
        max=1.0,
    )

    initial_temperature: FloatProperty(
        name="Initial Temperature",
        description="Starting temperature in Kelvin (293.15K = 20°C)",
        default=293.15,
        min=200.0,
        max=400.0,
    )

# =============================================================================
# Chemical Composition
# =============================================================================

class LoreChemicalProperties(PropertyGroup):
    """Chemical composition and combustion properties"""

    chemical_formula: StringProperty(
        name="Chemical Formula",
        description="Chemical formula (e.g., C6H10O5 for wood cellulose)",
        default="",
    )

    is_combustible: BoolProperty(
        name="Combustible",
        description="Can this material burn?",
        default=False,
    )

    oxidation_rate: FloatProperty(
        name="Oxidation Rate",
        description="How fast it oxidizes (0=inert, 10=explosive)",
        default=3.0,
        min=0.0,
        max=10.0,
    )

    heat_of_combustion: FloatProperty(
        name="Heat of Combustion",
        description="Energy released when burned in kJ/mol",
        default=2800.0,
        min=0.0,
        max=10000.0,
    )

    oxygen_required: FloatProperty(
        name="Oxygen Required",
        description="Moles of O2 needed per mole of fuel",
        default=6.0,
        min=0.0,
        max=20.0,
    )

    combustion_efficiency: FloatProperty(
        name="Combustion Efficiency",
        description="How complete the burn is (0=smoky, 1=clean)",
        default=0.95,
        min=0.0,
        max=1.0,
    )

    soot_production_rate: FloatProperty(
        name="Soot Production",
        description="Carbon particle generation (0=clean, 1=sooty)",
        default=0.15,
        min=0.0,
        max=1.0,
    )

    ash_residue_fraction: FloatProperty(
        name="Ash Residue",
        description="Solid residue after burning (0-1)",
        default=0.05,
        min=0.0,
        max=1.0,
    )

# =============================================================================
# Volumetric Fire Properties
# =============================================================================

class LoreVolumetricFireProperties(PropertyGroup):
    """GPU-based volumetric fire simulation properties"""

    grid_resolution_x: IntProperty(
        name="Grid Width",
        description="Grid resolution X (power of 2)",
        default=64,
        min=32,
        max=256,
    )

    grid_resolution_y: IntProperty(
        name="Grid Height",
        description="Grid resolution Y (power of 2)",
        default=128,
        min=32,
        max=512,
    )

    grid_resolution_z: IntProperty(
        name="Grid Depth",
        description="Grid resolution Z (power of 2)",
        default=64,
        min=32,
        max=256,
    )

    cell_size: FloatProperty(
        name="Cell Size",
        description="Voxel size in meters",
        default=0.1,
        min=0.01,
        max=0.5,
    )

    source_temperature: FloatProperty(
        name="Source Temperature",
        description="Fuel injection temperature in Kelvin",
        default=1500.0,
        min=800.0,
        max=2000.0,
    )

    source_fuel_rate: FloatProperty(
        name="Fuel Rate",
        description="Fuel injection rate in kg/s",
        default=0.1,
        min=0.001,
        max=10.0,
    )

    buoyancy_coefficient: FloatProperty(
        name="Buoyancy",
        description="Thermal lift strength",
        default=1.0,
        min=0.0,
        max=2.0,
    )

    vorticity_strength: FloatProperty(
        name="Turbulence",
        description="Turbulence amount (0-1)",
        default=0.3,
        min=0.0,
        max=1.0,
    )

    emission_strength: FloatProperty(
        name="Emission Strength",
        description="Light emission intensity",
        default=5.0,
        min=0.0,
        max=20.0,
    )

# =============================================================================
# Volumetric Fluid Properties
# =============================================================================

class LoreVolumetricFluidProperties(PropertyGroup):
    """GPU-based volumetric fluid simulation properties"""

    fluid_type: EnumProperty(
        name="Fluid Type",
        description="Preset fluid type",
        items=[
            ('WATER', "Water", "Standard water (1000 kg/m³, low viscosity)"),
            ('OIL', "Oil", "Viscous oil (850 kg/m³, medium viscosity)"),
            ('BLOOD', "Blood", "Non-Newtonian blood (1060 kg/m³)"),
            ('ACID', "Acid", "Corrosive acid (1200 kg/m³, emissive green)"),
            ('LAVA', "Lava", "Molten rock (3100 kg/m³, very viscous, emissive)"),
            ('CUSTOM', "Custom", "Custom fluid properties"),
        ],
        default='WATER',
    )

    grid_resolution_x: IntProperty(
        name="Grid Width",
        description="Grid resolution X (power of 2)",
        default=128,
        min=64,
        max=256,
    )

    grid_resolution_y: IntProperty(
        name="Grid Height",
        description="Grid resolution Y (power of 2)",
        default=64,
        min=64,
        max=256,
    )

    grid_resolution_z: IntProperty(
        name="Grid Depth",
        description="Grid resolution Z (power of 2)",
        default=128,
        min=64,
        max=256,
    )

    cell_size: FloatProperty(
        name="Cell Size",
        description="Voxel size in meters",
        default=0.05,
        min=0.02,
        max=0.2,
    )

    fluid_density: FloatProperty(
        name="Density",
        description="Fluid mass per volume in kg/m³",
        default=1000.0,
        min=500.0,
        max=5000.0,
    )

    dynamic_viscosity: FloatProperty(
        name="Viscosity",
        description="Fluid thickness in Pa·s (water=0.001, honey=10)",
        default=0.001,
        min=0.0001,
        max=500.0,
    )

    surface_tension: FloatProperty(
        name="Surface Tension",
        description="Surface curvature force in N/m",
        default=0.0728,
        min=0.01,
        max=0.5,
    )

    refraction_index: FloatProperty(
        name="Refraction Index",
        description="Light bending (water=1.333, glass=1.5)",
        default=1.333,
        min=1.0,
        max=2.0,
    )

    enable_foam: BoolProperty(
        name="Enable Foam",
        description="Generate foam particles at high turbulence",
        default=True,
    )

    enable_splashes: BoolProperty(
        name="Enable Splashes",
        description="Generate splash droplets",
        default=True,
    )

    enable_caustics: BoolProperty(
        name="Enable Caustics",
        description="Light caustics rendering (expensive)",
        default=True,
    )

# =============================================================================
# Registration
# =============================================================================

classes = [
    LoreStructuralMaterialProperties,
    LoreFractureProperties,
    LoreThermalProperties,
    LoreChemicalProperties,
    LoreVolumetricFireProperties,
    LoreVolumetricFluidProperties,
]

def register():
    """Register all property classes"""
    for cls in classes:
        bpy.utils.register_class(cls)

    # Attach properties to Blender types
    bpy.types.Material.lore_structural = PointerProperty(type=LoreStructuralMaterialProperties)
    bpy.types.Material.lore_thermal = PointerProperty(type=LoreThermalProperties)
    bpy.types.Material.lore_chemical = PointerProperty(type=LoreChemicalProperties)

    bpy.types.Object.lore_fracture = PointerProperty(type=LoreFractureProperties)
    bpy.types.Object.lore_volumetric_fire = PointerProperty(type=LoreVolumetricFireProperties)
    bpy.types.Object.lore_volumetric_fluid = PointerProperty(type=LoreVolumetricFluidProperties)

def unregister():
    """Unregister all property classes"""
    # Remove properties from Blender types
    del bpy.types.Material.lore_structural
    del bpy.types.Material.lore_thermal
    del bpy.types.Material.lore_chemical
    del bpy.types.Object.lore_fracture
    del bpy.types.Object.lore_volumetric_fire
    del bpy.types.Object.lore_volumetric_fluid

    # Unregister classes
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)