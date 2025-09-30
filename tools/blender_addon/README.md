# Lore Engine Tools - Professional Blender Addon

**Version:** 1.0.0
**Blender Compatibility:** 4.0.0+
**License:** Commercial (Sellable)

## Overview

Lore Engine Tools is a comprehensive, professional-grade Blender addon for creating game-ready assets with realistic physics, thermal, chemical, and structural properties. Perfect for game developers creating assets for the Lore Engine or any game engine requiring detailed material metadata.

## Features

### 🔨 Material Property Editor
- **Structural Properties**: Young's modulus, density, fracture toughness, yield strength
- **Thermal Properties**: Specific heat, thermal conductivity, melting/boiling points
- **Chemical Composition**: Chemical formulas, combustion properties, reaction rates
- **Physically Accurate**: All properties based on real-world measurements in SI units

### 💥 Advanced Fracture Generation
- **Voronoi Fracture**: Generate realistic fracture patterns with customizable cell counts
- **Multiple Patterns**:
  - Random (rocks, ceramics)
  - Radial (glass, ice impacts)
  - Grid (concrete, masonry)
  - Splinter (wood, bone)
- **Lloyd's Relaxation**: Uniform cell distribution algorithm
- **Procedural Cracks**: Add surface detail with procedural crack generation
- **Batch Processing**: Pre-compute multiple fracture variations for runtime use

### 📚 Material Preset Library
- **Steel**: Structural metal (200 GPa Young's modulus, 1811K melting point)
- **Wood**: Combustible cellulose (C6H10O5 formula, 573K ignition)
- **Concrete**: Brittle structural (30 GPa modulus, grid fracture pattern)
- **Glass**: Transparent brittle (radial fracture, low impact threshold)
- **Aluminum**: Light metal (high thermal conductivity, ductile)
- **Plastic**: ABS polymer (combustible, low melting point)

### 🌊 Volumetric Effects Support
- **Volumetric Fire**: GPU Navier-Stokes fire simulation parameters
- **Volumetric Fluids**: Water, oil, blood, acid, lava properties
- **Grid Configuration**: Customizable resolution and cell size

### 📦 Export & Validation
- **glTF 2.0 Export**: Embed all metadata in glTF 'extras' field
- **Validation**: Check for errors before export
- **Blender Integration**: Seamless integration with Blender's native workflow

## Installation

1. Download `lore_engine_tools.zip`
2. Open Blender → Edit → Preferences → Add-ons
3. Click "Install..." and select the zip file
4. Enable "Lore Engine Tools" in the addon list
5. Find the tools in 3D Viewport → Sidebar (N key) → "Lore Engine" tab

## Usage

### Quick Start: Apply Material Preset

1. Select an object with a material
2. Open Lore Engine sidebar (N key)
3. Navigate to "Material Presets" panel
4. Click on a preset (e.g., "Steel")
5. All properties are automatically configured!

### Generate Fracture

1. Select a mesh object
2. Open "Fracture Tools" panel
3. Enable "Enable Fracture"
4. Set "Voronoi Cells" count (higher = more pieces)
5. Choose fracture pattern (Random, Radial, Grid, Splinter)
6. Click "Voronoi Fracture"
7. Original object is hidden, fragments are created

### Pre-Compute Fracture Variations

1. Select fracturable object
2. Click "Batch Fracture Variations"
3. Set number of variations (5-20 recommended)
4. Addon generates multiple unique fracture patterns
5. Export all variations for runtime selection

### Export to Game Engine

1. Configure all properties for your scene
2. Click "Validate Properties" to check for errors
3. File → Export → Lore Engine glTF
4. Choose format (GLB recommended for games)
5. All Lore Engine metadata is embedded in glTF extras

## Property Reference

### Structural Properties
- **Young's Modulus** (MPa): Material stiffness
- **Poisson's Ratio**: Lateral vs axial strain
- **Density** (kg/m³): Mass per volume
- **Fracture Toughness** (MPa√m): Crack resistance
- **Yield Strength** (MPa): Plastic deformation threshold
- **Ultimate Strength** (MPa): Maximum stress before failure
- **Hardness** (HV): Vickers hardness
- **Ductility** (0-1): Ability to deform plastically
- **Damping Coefficient** (0-1): Vibration energy loss

### Thermal Properties
- **Specific Heat Capacity** (J/(kg·K)): Energy to raise 1kg by 1K
- **Thermal Conductivity** (W/(m·K)): Heat flow rate
- **Thermal Expansion** (1/K): Expansion per degree
- **Melting Point** (K): Solid → liquid transition
- **Boiling Point** (K): Liquid → gas transition
- **Ignition Temperature** (K): Auto-ignition (0 = non-flammable)
- **Emissivity** (0-1): Thermal radiation efficiency
- **Initial Temperature** (K): Starting temperature

### Chemical Properties
- **Chemical Formula**: E.g., "C6H10O5" for wood cellulose
- **Is Combustible**: Can material burn?
- **Oxidation Rate** (0-10): Burning speed (0=inert, 10=explosive)
- **Heat of Combustion** (kJ/mol): Energy released when burned
- **Oxygen Required** (moles): O2 needed per mole of fuel
- **Combustion Efficiency** (0-1): Burn completeness
- **Soot Production** (0-1): Carbon particles (0=clean, 1=sooty)
- **Ash Residue** (0-1): Solid leftover after burning

### Fracture Properties
- **Voronoi Cell Count**: Number of pieces (3-500)
- **Fracture Pattern**: Random, Radial, Grid, Splinter
- **Randomness** (0-1): Cell distribution chaos
- **Fracture Seed**: Random seed for reproducibility
- **Minimum Piece Volume** (m³): Filter small fragments
- **Crack Speed** (m/s): Propagation velocity
- **Impact Threshold** (J): Energy to trigger fracture
- **Shatter Completely**: Instant vs progressive fracture

## Technical Details

### Fracture Algorithms

**Voronoi Tessellation**:
- 3D Voronoi cell generation with customizable patterns
- Lloyd's relaxation for uniform distribution
- Integrates with Blender's Cell Fracture addon
- Fallback to manual slicing if addon unavailable

**Pattern Types**:
- **Random**: Pure random distribution (rocks, ceramics)
- **Grid**: Aligned chunks with jitter (concrete, masonry)
- **Radial**: Spherical layers from center (glass impact, ice)
- **Splinter**: Elongated along axis (wood, bone)

**Procedural Cracks**:
- Surface vertex displacement
- Random walk crack paths
- Tapered depth for realistic appearance
- Controllable crack count, depth, and width

### Export Format

Metadata is exported in glTF 2.0 `extras` field:

```json
{
  "materials": [{
    "name": "Steel",
    "extras": {
      "lore_structural_material": {
        "youngs_modulus": 200000.0,
        "density": 7850.0,
        ...
      },
      "lore_thermal_properties": {...},
      "lore_chemical_composition": {...}
    }
  }],
  "nodes": [{
    "name": "BreakableWall",
    "extras": {
      "lore_fracture_properties": {
        "voronoi_cell_count": 30,
        "fracture_pattern": "GRID",
        ...
      }
    }
  }]
}
```

## Tips & Best Practices

1. **Start with Presets**: Use material presets as starting points, then customize
2. **Validate Before Export**: Always run validation to catch errors
3. **LOD Considerations**: Use lower Voronoi cell counts for distant objects
4. **Batch Fracture**: Pre-compute 5-10 variations for runtime diversity
5. **Realistic Values**: Use physically accurate values for best visual results
6. **Test Iteratively**: Test fracture patterns on simple geometry first

## Troubleshooting

**Fracture Not Working?**
- Ensure "Enable Fracture" is checked
- Check Voronoi cell count is >= 3
- Try enabling Cell Fracture addon: Edit → Preferences → Add-ons → "Cell Fracture"

**Export Errors?**
- Run "Validate Properties" first
- Check density and Young's modulus are > 0
- Ensure melting point < boiling point

**Performance Issues?**
- Reduce Voronoi cell count for complex meshes
- Use batch processing for multiple variations
- Lower grid resolutions for volumetric effects

## Support & Documentation

- **Full Documentation**: See `docs/blender_metadata_guide.md` in Lore Engine repository
- **GitHub**: [github.com/lore-engine](https://github.com/lore-engine)
- **Issues**: Report bugs on GitHub Issues

## Changelog

### Version 1.0.0 (2025)
- Initial release
- Material property editor (structural, thermal, chemical)
- Advanced fracture generation (Voronoi, 4 patterns, Lloyd's relaxation)
- Procedural crack generation
- Batch fracture variations
- Material preset library (6 materials)
- glTF 2.0 export with metadata
- Property validation
- Professional UI panels

## License

**Commercial License** - This addon may be distributed and sold.

Copyright © 2025 Lore Engine Development Team

---

*Create realistic, physically-accurate game assets with Lore Engine Tools.*