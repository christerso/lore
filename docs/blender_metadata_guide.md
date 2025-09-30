# Lore Engine - Blender Metadata Guide

## Overview

This guide documents all custom metadata properties that should be set in Blender for models to work correctly with the Lore Engine's physics, thermal, chemical, and structural systems.

**Target Audience**: 3D artists using Blender to create game assets
**Integration Method**: Custom properties on Blender objects/materials
**File Format**: Properties exported via glTF 2.0 extras or custom metadata

---

## Table of Contents

1. [Structural Material Properties](#1-structural-material-properties)
2. [Fracture Properties](#2-fracture-properties)
3. [Thermal Properties](#3-thermal-properties)
4. [Chemical Composition](#4-chemical-composition)
5. [Combustion Properties](#5-combustion-properties)
6. [Volumetric Fire Properties](#6-volumetric-fire-properties)
7. [Volumetric Smoke Properties](#7-volumetric-smoke-properties)
8. [Volumetric Fluid Properties](#8-volumetric-fluid-properties)
9. [Common Material Presets](#9-common-material-presets)

---

## 1. Structural Material Properties

These properties define how materials deform and break under stress.

### Property Group: `lore_structural_material`

| Property | Type | Range | Unit | Description | Example (Steel) |
|----------|------|-------|------|-------------|-----------------|
| `youngs_modulus` | Float | 0.1 - 500000 | MPa | Material stiffness (resistance to elastic deformation) | 200000 |
| `poisson_ratio` | Float | 0.0 - 0.5 | - | Lateral strain vs axial strain ratio | 0.3 |
| `density` | Float | 10 - 20000 | kg/m³ | Mass per unit volume | 7850 |
| `fracture_toughness` | Float | 0.01 - 200 | MPa√m | Resistance to crack propagation | 50 |
| `yield_strength` | Float | 0.1 - 5000 | MPa | Stress at which plastic deformation begins | 250 |
| `ultimate_strength` | Float | 1 - 6000 | MPa | Maximum stress before failure | 400 |
| `hardness` | Float | 10 - 10000 | HV | Vickers hardness (resistance to indentation) | 200 |
| `ductility` | Float | 0.0 - 1.0 | - | Ability to deform plastically (0=brittle, 1=ductile) | 0.8 |
| `damping_coefficient` | Float | 0.0 - 1.0 | - | Energy dissipation during vibration | 0.01 |

**Common Values**:
- **Steel**: `youngs_modulus=200000`, `density=7850`, `fracture_toughness=50`, `ductility=0.8`
- **Wood**: `youngs_modulus=11000`, `density=600`, `fracture_toughness=5`, `ductility=0.3`
- **Concrete**: `youngs_modulus=30000`, `density=2400`, `fracture_toughness=1.5`, `ductility=0.1`
- **Glass**: `youngs_modulus=70000`, `density=2500`, `fracture_toughness=0.7`, `ductility=0.0`

---

## 2. Fracture Properties

These properties control how objects break and fragment.

### Property Group: `lore_fracture_properties`

| Property | Type | Range | Unit | Description | Example |
|----------|------|-------|------|-------------|---------|
| `fracture_enabled` | Bool | - | - | Can this object fracture? | True |
| `voronoi_cell_count` | Int | 3 - 500 | - | Number of fracture pieces | 25 |
| `fracture_seed` | Int | 0 - 99999 | - | Random seed for reproducible fractures | 12345 |
| `fracture_randomness` | Float | 0.0 - 1.0 | - | Cell distribution randomness (0=uniform, 1=chaotic) | 0.8 |
| `minimum_piece_volume` | Float | 0.0001 - 1.0 | m³ | Smallest allowed fragment size | 0.01 |
| `damage_propagation_speed` | Float | 0.1 - 100.0 | m/s | How fast cracks spread through material | 10.0 |
| `impact_threshold_energy` | Float | 0.1 - 10000 | J | Energy needed to start fracture | 100.0 |
| `shatter_completely` | Bool | - | - | Break into all pieces at once vs progressive fracture | False |

**Fracture Patterns**:
- **Radial** (`fracture_pattern=radial`): Cracks radiate from impact point (glass, ice)
- **Grid** (`fracture_pattern=grid`): Rectangular chunks (concrete, masonry)
- **Splinters** (`fracture_pattern=splinter`): Long shards (wood, bone)
- **Random** (`fracture_pattern=random`): Irregular chunks (rock, ceramics)

---

## 3. Thermal Properties

These properties define heat transfer and temperature behavior.

### Property Group: `lore_thermal_properties`

| Property | Type | Range | Unit | Description | Example (Steel) |
|----------|------|-------|------|-------------|-----------------|
| `specific_heat_capacity` | Float | 100 - 5000 | J/(kg·K) | Energy to raise 1kg by 1K | 500 |
| `thermal_conductivity` | Float | 0.01 - 500 | W/(m·K) | Rate of heat flow through material | 50 |
| `thermal_expansion_coeff` | Float | 0 - 0.0001 | 1/K | Expansion per degree Kelvin | 0.000012 |
| `melting_point` | Float | 200 - 4000 | K | Temperature at which solid → liquid | 1811 |
| `boiling_point` | Float | 300 - 6000 | K | Temperature at which liquid → gas | 3134 |
| `ignition_temperature` | Float | 0 - 2000 | K | Temperature at which material ignites (0=non-flammable) | 0 |
| `emissivity` | Float | 0.0 - 1.0 | - | How efficiently material radiates heat (0=mirror, 1=blackbody) | 0.7 |
| `initial_temperature` | Float | 200 - 400 | K | Starting temperature (293.15K = 20°C) | 293.15 |

**Common Values**:
- **Steel**: `specific_heat=500`, `conductivity=50`, `melting=1811`, `emissivity=0.7`
- **Wood**: `specific_heat=1700`, `conductivity=0.15`, `ignition=573`, `emissivity=0.9`
- **Concrete**: `specific_heat=880`, `conductivity=1.4`, `melting=1923`, `emissivity=0.95`
- **Gasoline**: `specific_heat=2220`, `conductivity=0.15`, `ignition=553`, `boiling=483`

---

## 4. Chemical Composition

These properties define what elements make up the material.

### Property Group: `lore_chemical_composition`

| Property | Type | Range | Description | Example (Wood) |
|----------|------|-------|-------------|----------------|
| `chemical_formula` | String | - | Chemical formula | "C6H10O5" |
| `is_combustible` | Bool | - | Can material burn? | True |
| `oxidation_rate` | Float | 0.0 - 10.0 | How fast it oxidizes (0=inert, 10=explosive) | 3.0 |
| `heat_of_combustion` | Float | 0 - 10000 | kJ/mol released when burned | 2800 |
| `oxygen_required` | Float | 0 - 20 | Moles of O2 needed per mole of fuel | 6.0 |
| `combustion_efficiency` | Float | 0.0 - 1.0 | How complete the burn is (0=smoky, 1=clean) | 0.95 |
| `soot_production_rate` | Float | 0.0 - 1.0 | Carbon particle generation (0=clean, 1=sooty) | 0.15 |
| `ash_residue_fraction` | Float | 0.0 - 1.0 | Solid residue after burning | 0.05 |

**Element Proportions** (array of):
```json
{
    "element": "C",  // Chemical symbol
    "molar_ratio": 6.0,  // Moles per formula unit
    "mass_fraction": 0.444  // 44.4% by mass
}
```

**Preset Formulas**:
- **Wood (Cellulose)**: `C6H10O5`, `oxygen_required=6.0`, `heat=2800`
- **Gasoline (Octane)**: `C8H18`, `oxygen_required=12.5`, `heat=5470`
- **Gunpowder**: `KNO3_C_S`, `oxygen_required=0` (self-oxidizing), `heat=3000`
- **Steel**: `Fe_C`, `is_combustible=false`

---

## 5. Combustion Properties

These properties control active fire behavior.

### Property Group: `lore_combustion`

| Property | Type | Range | Unit | Description | Default |
|----------|------|-------|------|-------------|---------|
| `fuel_mass` | Float | 0.01 - 1000 | kg | Mass of burnable fuel | 1.0 |
| `fuel_consumption_rate` | Float | 0.001 - 10 | kg/s | Burn rate | 0.01 |
| `flame_temperature` | Float | 800 - 1800 | K | Current flame temperature | 1200 |
| `peak_temperature` | Float | 1000 - 2000 | K | Maximum achievable temperature | 1500 |
| `flame_height` | Float | 0.1 - 10.0 | m | Visible flame height | 1.0 |
| `flame_radius` | Float | 0.1 - 5.0 | m | Flame base radius | 0.5 |
| `spread_rate` | Float | 0.01 - 2.0 | m/s | How fast fire spreads | 0.1 |
| `ignition_radius` | Float | 0.5 - 5.0 | m | Distance that can ignite neighbors | 1.0 |
| `smoke_generation_rate` | Float | 0.0 - 5.0 | - | Smoke particle spawn multiplier | 1.0 |
| `ember_generation_rate` | Float | 0.0 - 5.0 | - | Ember particle spawn multiplier | 1.0 |
| `heat_distortion_strength` | Float | 0.0 - 2.0 | - | Air wobble effect intensity | 1.0 |

---

## 6. Volumetric Fire Properties

For volumetric GPU-based fire simulation.

### Property Group: `lore_volumetric_fire`

| Property | Type | Range | Unit | Description | Default |
|----------|------|-------|------|-------------|---------|
| `grid_resolution_x` | Int | 32 - 256 | - | Grid width (power of 2) | 64 |
| `grid_resolution_y` | Int | 32 - 512 | - | Grid height (power of 2) | 128 |
| `grid_resolution_z` | Int | 32 - 256 | - | Grid depth (power of 2) | 64 |
| `cell_size` | Float | 0.01 - 0.5 | m | Voxel size | 0.1 |
| `source_temperature` | Float | 800 - 2000 | K | Fuel injection temperature | 1500 |
| `source_fuel_rate` | Float | 0.001 - 10 | kg/s | Fuel injection rate | 0.1 |
| `buoyancy_coefficient` | Float | 0.0 - 2.0 | - | Thermal lift strength | 1.0 |
| `vorticity_strength` | Float | 0.0 - 1.0 | - | Turbulence amount | 0.3 |
| `emission_strength` | Float | 0.0 - 20.0 | - | Light emission intensity | 5.0 |
| `pressure_iterations` | Int | 20 - 100 | - | Jacobi solver iterations | 40 |
| `raymarch_steps` | Int | 32 - 256 | - | Rendering ray samples | 128 |

**Fire Presets**:
- **Campfire**: `resolution=48x96x48`, `cell=0.08`, `temp=1200K`, `fuel=0.05kg/s`
- **Building Fire**: `resolution=128x256x128`, `cell=0.2`, `temp=1400K`, `fuel=1.0kg/s`
- **Torch**: `resolution=32x64x32`, `cell=0.05`, `temp=1300K`, `fuel=0.01kg/s`

---

## 7. Volumetric Smoke Properties

For volumetric GPU-based smoke simulation.

### Property Group: `lore_volumetric_smoke`

| Property | Type | Range | Unit | Description | Default |
|----------|------|-------|------|-------------|---------|
| `grid_resolution_x` | Int | 64 - 512 | - | Grid width (power of 2) | 128 |
| `grid_resolution_y` | Int | 64 - 512 | - | Grid height (power of 2) | 128 |
| `grid_resolution_z` | Int | 64 - 512 | - | Grid depth (power of 2) | 128 |
| `cell_size` | Float | 0.02 - 5.0 | m | Voxel size | 0.1 |
| `opacity` | Float | 0.0 - 1.0 | - | Smoke darkness (0=transparent, 1=opaque) | 0.8 |
| `source_radius` | Float | 0.1 - 10.0 | m | Smoke source size | 1.0 |
| `rise_speed` | Float | 0.0 - 5.0 | m/s | Upward velocity | 1.0 |
| `diffusion_rate` | Float | 0.0 - 1.0 | - | Smoke spread rate | 0.1 |
| `dissipation_rate` | Float | 0.9 - 1.0 | - | Smoke decay per frame (0.98 = slow fade) | 0.98 |
| `buoyancy_strength` | Float | 0.0 - 2.0 | - | Hot air rises multiplier | 1.0 |
| `noise_frequency` | Float | 0.1 - 5.0 | - | Turbulence detail frequency | 1.0 |
| `noise_octaves` | Int | 1 - 8 | - | Noise layers for detail | 4 |
| `restir_spatial_samples` | Int | 4 - 32 | - | ReSTIR lighting samples | 8 |
| `enable_restir_gi` | Bool | - | - | High-quality lighting (expensive) | True |

**Smoke Color** (RGB):
- **Dark Gray** (fire smoke): `(0.2, 0.2, 0.2)`
- **Black** (explosion): `(0.1, 0.1, 0.1)`
- **White** (steam): `(0.9, 0.9, 0.9)`
- **Yellow-Brown** (dust): `(0.6, 0.5, 0.3)`

---

## 8. Volumetric Fluid Properties

For volumetric GPU-based fluid simulation (water, oil, blood, lava).

### Property Group: `lore_volumetric_fluid`

| Property | Type | Range | Unit | Description | Example (Water) |
|----------|------|-------|------|-------------|-----------------|
| `grid_resolution_x` | Int | 64 - 256 | - | Grid width (power of 2) | 128 |
| `grid_resolution_y` | Int | 64 - 256 | - | Grid height (power of 2) | 64 |
| `grid_resolution_z` | Int | 64 - 256 | - | Grid depth (power of 2) | 128 |
| `cell_size` | Float | 0.02 - 0.2 | m | Voxel size | 0.05 |
| `fluid_density` | Float | 500 - 5000 | kg/m³ | Fluid mass per volume | 1000 |
| `dynamic_viscosity` | Float | 0.0001 - 500 | Pa·s | Fluid thickness (water=0.001, honey=10) | 0.001 |
| `surface_tension` | Float | 0.01 - 0.5 | N/m | Surface curvature force | 0.0728 |
| `refraction_index` | Float | 1.0 - 2.0 | - | Light bending (water=1.333, glass=1.5) | 1.333 |
| `color_r` | Float | 0.0 - 1.0 | - | Red channel | 0.1 |
| `color_g` | Float | 0.0 - 1.0 | - | Green channel | 0.3 |
| `color_b` | Float | 0.0 - 1.0 | - | Blue channel | 0.8 |
| `color_a` | Float | 0.0 - 1.0 | - | Alpha (transparency) | 0.7 |
| `enable_foam` | Bool | - | - | Generate foam particles | True |
| `enable_splashes` | Bool | - | - | Generate splash droplets | True |
| `enable_waves` | Bool | - | - | Surface wave simulation | True |
| `enable_caustics` | Bool | - | - | Light caustics rendering | True |

**Fluid Presets**:
- **Water**: `density=1000`, `viscosity=0.001`, `tension=0.0728`, `n=1.333`, `color=(0.1,0.3,0.8)`
- **Oil**: `density=850`, `viscosity=0.05`, `tension=0.032`, `n=1.47`, `color=(0.1,0.08,0.05)`
- **Blood**: `density=1060`, `viscosity=0.004`, `tension=0.058`, `n=1.35`, `color=(0.6,0.05,0.05)`
- **Lava**: `density=3100`, `viscosity=100`, `tension=0.4`, `n=1.6`, `color=(1.0,0.3,0.05)`, `emissive=true`

---

## 9. Common Material Presets

### Complete Material Definitions

#### Steel (Structural Metal)
```
Structural: youngs_modulus=200000, density=7850, fracture_toughness=50, ductility=0.8
Thermal: specific_heat=500, conductivity=50, melting=1811K, emissivity=0.7
Chemical: formula="Fe_C", is_combustible=false
Fracture: voronoi_cells=20, pattern=random, threshold=500J
```

#### Wood (Combustible Organic)
```
Structural: youngs_modulus=11000, density=600, fracture_toughness=5, ductility=0.3
Thermal: specific_heat=1700, conductivity=0.15, ignition=573K, melting=0
Chemical: formula="C6H10O5", is_combustible=true, oxygen_required=6.0, heat=2800kJ
Combustion: fuel_mass=10kg, consumption_rate=0.05kg/s, flame_temp=1200K
Fracture: voronoi_cells=15, pattern=splinter, threshold=50J
```

#### Concrete (Brittle Structural)
```
Structural: youngs_modulus=30000, density=2400, fracture_toughness=1.5, ductility=0.1
Thermal: specific_heat=880, conductivity=1.4, melting=1923K, emissivity=0.95
Chemical: formula="CaCO3_SiO2", is_combustible=false
Fracture: voronoi_cells=30, pattern=grid, threshold=200J
```

#### Glass (Transparent Brittle)
```
Structural: youngs_modulus=70000, density=2500, fracture_toughness=0.7, ductility=0.0
Thermal: specific_heat=840, conductivity=1.0, melting=1873K, emissivity=0.9
Chemical: formula="SiO2", is_combustible=false
Fracture: voronoi_cells=50, pattern=radial, threshold=20J, shatter_completely=true
```

#### Gasoline (Flammable Liquid)
```
Structural: density=750 (liquid)
Thermal: specific_heat=2220, conductivity=0.15, ignition=553K, boiling=483K
Chemical: formula="C8H18", is_combustible=true, oxygen_required=12.5, heat=5470kJ
Volumetric_Fluid: density=750, viscosity=0.0006, refraction=1.39, color=(0.9,0.7,0.3)
```

---

## 10. Property Hierarchy

Properties should be attached to Blender objects in this hierarchy:

```
Object (Mesh)
├── lore_structural_material (on Material)
├── lore_fracture_properties (on Object)
├── lore_thermal_properties (on Material)
├── lore_chemical_composition (on Material)
└── lore_combustion (on Object, if flammable)

Volumetric Effects (separate Empty objects)
├── lore_volumetric_fire (on Empty with bounds)
├── lore_volumetric_smoke (on Empty with bounds)
└── lore_volumetric_fluid (on Empty with bounds)
```

---

## 11. Export Format

Properties should be exported in glTF 2.0 format using the `extras` field:

```json
{
  "materials": [
    {
      "name": "Steel",
      "extras": {
        "lore_structural_material": {
          "youngs_modulus": 200000.0,
          "density": 7850.0,
          "fracture_toughness": 50.0,
          "ductility": 0.8
        },
        "lore_thermal_properties": {
          "specific_heat_capacity": 500.0,
          "thermal_conductivity": 50.0,
          "melting_point": 1811.0
        },
        "lore_chemical_composition": {
          "chemical_formula": "Fe_C",
          "is_combustible": false
        }
      }
    }
  ],
  "nodes": [
    {
      "name": "SteelBeam",
      "extras": {
        "lore_fracture_properties": {
          "fracture_enabled": true,
          "voronoi_cell_count": 20,
          "impact_threshold_energy": 500.0
        }
      }
    }
  ]
}
```

---

## 12. Validation Rules

The Lore Engine loader will validate:

1. **Required Properties**: `density`, `youngs_modulus` must be present for structural objects
2. **Range Validation**: All values must be within specified ranges
3. **Consistency**: If `is_combustible=true`, then `ignition_temperature` must be > 0
4. **Units**: All values must be in SI units (meters, kg, Kelvin, Pascals)
5. **Grid Resolutions**: Must be powers of 2 (32, 64, 128, 256)

---

## 13. Best Practices

1. **Start with Presets**: Use common material presets and tweak as needed
2. **Real-World Values**: Use physically accurate values for realistic behavior
3. **Performance**: Lower grid resolutions for background objects, higher for hero assets
4. **Testing**: Test fracture patterns in isolation before adding to full scenes
5. **LOD**: Set appropriate LOD distances to maintain 60 FPS
6. **Validation**: Run the engine's metadata validator before final export

---

## Appendix A: SI Units Reference

| Quantity | Unit | Symbol | Notes |
|----------|------|--------|-------|
| Length | meter | m | - |
| Mass | kilogram | kg | - |
| Time | second | s | - |
| Temperature | Kelvin | K | 273.15K = 0°C, 293.15K = 20°C |
| Pressure/Stress | Pascal | Pa | 1 MPa = 1,000,000 Pa |
| Energy | Joule | J | 1 kJ = 1000 J |
| Power | Watt | W | 1 W = 1 J/s |
| Thermal Conductivity | - | W/(m·K) | - |
| Specific Heat | - | J/(kg·K) | - |
| Viscosity (Dynamic) | Pascal-second | Pa·s | Water ≈ 0.001 Pa·s |
| Surface Tension | Newton per meter | N/m | Water ≈ 0.0728 N/m |

---

## Appendix B: Temperature Conversion

| Celsius | Kelvin | Fahrenheit | Description |
|---------|--------|------------|-------------|
| -273°C | 0 K | -460°F | Absolute zero |
| 0°C | 273.15 K | 32°F | Water freezes |
| 20°C | 293.15 K | 68°F | Room temperature |
| 100°C | 373.15 K | 212°F | Water boils |
| 232°C | 505 K | 450°F | Paper ignites |
| 1000°C | 1273 K | 1832°F | Lava temperature |
| 1538°C | 1811 K | 2800°F | Steel melts |

**Formula**: `Kelvin = Celsius + 273.15`

---

*This guide is maintained alongside the Lore Engine codebase. Version 1.0 - 2025*