# Advanced Destruction System - Next Generation

## Beyond Basic Voronoi: State-of-the-Art Destruction

This document describes enhancements to make our destruction system rival AAA titles like Control, Teardown, and Red Faction Guerrilla.

---

## System 1: Pre-Fractured Meshes with Lazy Generation

### Problem with Runtime Voronoi
- Voronoi computation is expensive (10-50ms per tile)
- Breaks immersion during gameplay
- All breaks look procedurally random (no artistic control)

### Solution: Hybrid Pre-Fracture + Runtime System

```cpp
namespace lore::world {

// Fracture cache with multiple variants per mesh
struct FracturePlan {
    uint32_t plan_id;
    std::string source_mesh_path;

    // Pre-computed fracture data
    std::vector<FractureFragment> fragments;
    std::vector<FractureConnection> connections;  // Which pieces connect

    // Metadata for runtime
    math::Vec3 centroid;
    float structural_integrity = 1.0f;
    bool supports_progressive_damage = true;

    // Artist annotations (optional)
    std::vector<WeakPoint> weak_points;          // Hinge points, stress points
    std::vector<StructuralSupport> supports;     // Load-bearing elements
};

struct FractureFragment {
    std::vector<math::Vec3> vertices;
    std::vector<uint32_t> indices;
    std::vector<math::Vec2> uvs;
    std::vector<math::Vec3> normals;

    // NEW: Fragment metadata
    float mass;
    math::Vec3 centroid;
    math::Vec3 inertia_tensor;
    uint32_t material_id;

    // NEW: Connection info
    std::vector<uint32_t> connected_fragment_ids;  // Neighbors
    std::vector<FractureEdge> connection_edges;    // Shared edges
    float connection_strength = 1.0f;               // How hard to separate
};

struct FractureEdge {
    math::Vec3 start, end;
    float area;                  // Contact area
    float normal_strength;       // Resistance to pulling apart
    float shear_strength;        // Resistance to sliding
};

struct WeakPoint {
    math::Vec3 position;
    float threshold_damage;      // Damage needed to break here
    std::vector<uint32_t> affected_fragments;
};

// Fracture plan cache manager
class FracturePlanCache {
public:
    // Load pre-fractured mesh from disk (e.g., "meshes/Cube.fbx.fracture")
    const FracturePlan* load_fracture_plan(const std::string& mesh_path);

    // Generate fracture plan on-demand and cache to disk
    FracturePlan generate_and_cache(const std::string& mesh_path, const FractureConfig& config);

    // Get random variant (if multiple plans exist)
    const FracturePlan* get_random_variant(const std::string& mesh_path);

private:
    std::unordered_map<std::string, std::vector<FracturePlan>> cache_;
};

} // namespace lore::world
```

### Offline Pre-Fracture Tool

```bash
# Command-line tool to pre-fracture meshes
lore-fracture-tool Cube.fbx --output Cube.fbx.fracture --fragments 20 --variants 5

# Generates 5 different fracture patterns, saves to disk
# At runtime: instant loading, zero computation
```

**Benefits:**
- ✅ Zero runtime cost (load pre-computed fracture)
- ✅ Multiple variants per mesh (variety without procedural look)
- ✅ Artist can tweak fracture patterns in Blender/Maya
- ✅ Fallback to runtime Voronoi if no pre-fracture exists

---

## System 2: Directional Fracture Based on Impact

### Problem
- Random Voronoi doesn't respect impact direction
- Bullet hole should fracture differently than sledgehammer
- High-velocity impacts create different patterns than slow crushing

### Solution: Stress-Guided Fracture

```cpp
namespace lore::world {

enum class ImpactType {
    PointImpact,      // Bullet, arrow
    BluntForce,       // Hammer, bat
    Explosion,        // Grenade, rocket
    Cutting,          // Sword, axe
    Crushing,         // Heavy object falling
    Shearing          // Sliding force
};

struct ImpactData {
    math::Vec3 position;           // Impact point in world space
    math::Vec3 direction;          // Impact direction (normalized)
    float force;                   // Newtons
    ImpactType type;
    float impulse_duration;        // Short = bullet, long = crushing
};

// Enhanced fracture generator
class DirectionalFractureSystem {
public:
    std::vector<DebrisPiece> fracture_with_impact(
        const TileInstance& tile,
        const ImpactData& impact);

private:
    // Bias Voronoi points toward impact direction
    std::vector<math::Vec3> generate_stress_guided_points(
        const ImpactData& impact,
        const TileBounds& bounds);

    // Elongate fracture cells along stress lines
    void apply_stress_field(
        VoronoiDiagram& diagram,
        const ImpactData& impact);

    // Smaller fragments near impact, larger away
    void apply_fragment_size_gradient(
        std::vector<DebrisPiece>& fragments,
        const ImpactData& impact);
};

// Implementation: Stress-guided fracture points
std::vector<math::Vec3> generate_stress_guided_points(
    const ImpactData& impact,
    const TileBounds& bounds)
{
    std::vector<math::Vec3> points;

    // More fracture points near impact
    float base_fragments = 15;
    float impact_multiplier = std::clamp(impact.force / 1000.0f, 1.0f, 3.0f);
    int32_t num_fragments = static_cast<int32_t>(base_fragments * impact_multiplier);

    points.reserve(num_fragments);

    for (int32_t i = 0; i < num_fragments; ++i) {
        math::Vec3 point;

        // Bias toward impact point (exponential falloff)
        float impact_bias = std::exp(-float(i) / num_fragments * 3.0f);

        if (random_float() < impact_bias) {
            // Near impact point
            point = impact.position + random_in_sphere(0.3f);
        } else {
            // Elsewhere in bounds
            point = random_in_bounds(bounds);
        }

        // Align along impact direction (creates elongated fractures)
        if (impact.type == ImpactType::PointImpact) {
            math::Vec3 offset = impact.direction * random_float(-0.2f, 0.2f);
            point += offset;
        }

        points.push_back(point);
    }

    return points;
}

} // namespace lore::world
```

### Impact-Specific Fracture Patterns

**Bullet Impact (Point, High Velocity):**
- Small entry hole (2-3 fragments)
- Cone of spalling on exit side (10-15 fragments)
- Radial cracks from entry point

**Sledgehammer (Blunt, Medium Velocity):**
- Large depression at impact (5-8 fragments)
- Radial cracks spreading outward
- Few large chunks, not many small pieces

**Explosion (Omnidirectional):**
- Many small fragments (20-30)
- Outward velocity from center
- Relatively uniform size distribution

**Axe/Sword (Cutting):**
- Clean split along blade path
- Two large halves
- Minimal fragmentation

---

## System 3: Multi-Material Destruction

### Problem
- Real walls are composite: stone facade + wooden studs + drywall
- Breaking stone shouldn't look like breaking wood
- Interior and exterior should fracture differently

### Solution: Layered Material System

```cpp
namespace lore::world {

struct MaterialLayer {
    uint32_t material_id;            // PBR material
    float thickness;                 // Meters
    float density;                   // kg/m³
    float tensile_strength;          // MPa (resistance to pulling)
    float compressive_strength;      // MPa (resistance to crushing)
    float shear_strength;            // MPa (resistance to sliding)

    // Fracture behavior
    FractureBehavior behavior;
};

enum class FractureBehavior {
    Brittle,        // Glass, stone - shatters into many pieces
    Ductile,        // Metal - bends then tears
    Fibrous,        // Wood - splinters along grain
    Granular,       // Dirt, sand - crumbles
    Elastic         // Rubber - deforms and bounces back
};

// Multi-layer tile definition
struct LayeredTileDefinition : public TileDefinition {
    std::vector<MaterialLayer> layers;  // Front to back

    // Layer composition (e.g., "stone + wood + plaster")
    // When tile breaks, each layer fractures independently
};

// Layered fracture system
class LayeredFractureSystem {
public:
    struct LayeredDebris {
        std::vector<DebrisPiece> stone_pieces;
        std::vector<DebrisPiece> wood_pieces;
        std::vector<DebrisPiece> plaster_pieces;
        std::vector<ParticleEmitter> dust_emitters;  // Small particles
    };

    LayeredDebris fracture_layered(
        const LayeredTileDefinition& def,
        const ImpactData& impact);

private:
    // Fracture each layer with appropriate pattern
    std::vector<DebrisPiece> fracture_layer(
        const MaterialLayer& layer,
        const ImpactData& impact,
        float depth_in_tile);
};

} // namespace lore::world
```

### Example: Stone Wall with Wooden Studs

```cpp
LayeredTileDefinition castle_wall;
castle_wall.mesh_path = "meshes/CastleWall.fbx";

// Outer stone facade (0.2m thick)
MaterialLayer stone_facade;
stone_facade.material_id = MAT_ROUGH_STONE;
stone_facade.thickness = 0.2f;
stone_facade.density = 2400.0f;  // kg/m³
stone_facade.tensile_strength = 5.0f;  // MPa (stone is weak in tension)
stone_facade.compressive_strength = 50.0f;  // MPa (strong in compression)
stone_facade.behavior = FractureBehavior::Brittle;
castle_wall.layers.push_back(stone_facade);

// Wooden support beams (0.15m thick)
MaterialLayer wood_frame;
wood_frame.material_id = MAT_AGED_WOOD;
wood_frame.thickness = 0.15f;
wood_frame.density = 600.0f;
wood_frame.tensile_strength = 10.0f;
wood_frame.compressive_strength = 20.0f;
wood_frame.behavior = FractureBehavior::Fibrous;
castle_wall.layers.push_back(wood_frame);

// Inner plaster (0.05m thick)
MaterialLayer plaster;
plaster.material_id = MAT_PAINTED_PLASTER;
plaster.thickness = 0.05f;
plaster.density = 1200.0f;
plaster.tensile_strength = 2.0f;
plaster.compressive_strength = 8.0f;
plaster.behavior = FractureBehavior::Brittle;
castle_wall.layers.push_back(plaster);

// When cannon ball hits:
// 1. Stone facade shatters into 5-8 large angular chunks
// 2. Wooden beams splinter into 10-15 elongated pieces
// 3. Plaster crumbles into fine dust (particle system)
```

---

## System 4: Progressive Damage and Crack Propagation

### Problem
- Instant destruction is unrealistic
- Real structures develop cracks before failing
- Players should see warning signs before collapse

### Solution: Damage Accumulation System

```cpp
namespace lore::world {

enum class DamageState {
    Pristine,        // 100% health, no visible damage
    Scratched,       // 80-100%, superficial marks
    Cracked,         // 60-80%, visible cracks
    Damaged,         // 40-60%, large cracks, missing chunks
    Failing,         // 20-40%, structural failure imminent
    Critical,        // 0-20%, about to collapse
    Collapsed        // 0%, fully destroyed
};

struct TileDamage {
    float total_damage = 0.0f;              // Accumulated damage
    DamageState state = DamageState::Pristine;

    // Visible cracks (rendered as decals)
    std::vector<CrackPath> cracks;

    // Damage hotspots (areas of concentrated stress)
    std::vector<DamageHotspot> hotspots;

    // Time until structural failure (if load-bearing)
    float time_until_collapse = -1.0f;
};

struct CrackPath {
    std::vector<math::Vec3> points;         // Crack polyline
    float width;                            // Crack width in meters
    float depth;                            // How deep crack penetrates
    float growth_rate;                      // m/s (cracks spread over time)
    math::Vec3 stress_direction;            // Direction of stress causing crack
};

struct DamageHotspot {
    math::Vec3 position;
    float stress_level;                     // 0.0 = no stress, 1.0 = about to fail
    float radius;
    std::vector<uint32_t> affected_fragments;  // Which pieces will break
};

class ProgressiveDamageSystem {
public:
    // Apply damage without immediate fracture
    void apply_damage(TileInstance& tile, const ImpactData& impact, float damage_amount);

    // Update cracks (spread over time)
    void update_crack_propagation(float delta_time);

    // Check if tile should collapse due to accumulated damage
    bool should_collapse(const TileInstance& tile) const;

    // Render cracks as decals on intact mesh
    void render_damage_decals(const TileInstance& tile);

private:
    // Generate crack from impact point
    CrackPath generate_crack(const TileInstance& tile, const ImpactData& impact);

    // Spread crack based on stress
    void propagate_crack(CrackPath& crack, float delta_time);

    // Identify weak points from crack intersections
    std::vector<DamageHotspot> find_structural_failures(const TileDamage& damage);
};

} // namespace lore::world
```

### Crack Propagation Algorithm

```cpp
void ProgressiveDamageSystem::propagate_crack(CrackPath& crack, float delta_time) {
    if (crack.points.size() < 2) return;

    math::Vec3 crack_tip = crack.points.back();
    math::Vec3 crack_direction = normalize(crack_tip - crack.points[crack.points.size() - 2]);

    // Cracks follow stress lines and weak points
    math::Vec3 stress_vector = calculate_stress_at_point(crack_tip);

    // Bias toward stress direction
    math::Vec3 growth_direction = normalize(
        crack_direction * 0.7f + stress_vector * 0.3f
    );

    // Crack growth rate depends on stress level
    float stress_magnitude = length(stress_vector);
    float growth_distance = crack.growth_rate * delta_time * stress_magnitude;

    // Extend crack
    math::Vec3 new_point = crack_tip + growth_direction * growth_distance;
    crack.points.push_back(new_point);

    // Widen crack under high stress
    if (stress_magnitude > 0.8f) {
        crack.width = std::min(crack.width * 1.1f, 0.05f);  // Max 5cm wide
    }

    // Check if crack reached edge (tile fails)
    if (reached_tile_boundary(new_point)) {
        // Trigger structural failure
        initiate_collapse(crack);
    }
}
```

---

## System 5: Artist-Controlled Fracture

### Problem
- Pure procedural fracture lacks artistic intent
- Important structures (doors, arches) should break predictably
- Gameplay needs reliable destruction (breach doors, not walls)

### Solution: Fracture Annotations

```cpp
namespace lore::world {

// Artist places these in Blender/Maya as empty objects
struct FractureAnnotation {
    std::string name;
    AnnotationType type;
    math::Vec3 position;
    math::Quat rotation;
    float influence_radius;

    // Custom properties
    std::map<std::string, std::string> properties;
};

enum class AnnotationType {
    WeakPoint,           // "HingePoint", "StressPoint"
    StructuralSupport,   // Load-bearing, don't break easily
    FractureLine,        // Pre-defined split line
    DontFracture,        // Protected area (e.g., door frame)
    ForceFragment,       // Guarantee this piece breaks off
    ParticleEmitter      // Dust/debris spawn point
};

// Example: Castle gate
// Artist places annotations in Blender:
//   - "Hinge_Left" weak point at left hinge
//   - "Hinge_Right" weak point at right hinge
//   - "Reinforcement_Bar" as StructuralSupport
//   - "FractureLine_Vertical" to ensure it splits vertically

class AnnotationSystem {
public:
    // Load annotations from FBX/glTF metadata
    std::vector<FractureAnnotation> load_annotations(const std::string& mesh_path);

    // Modify Voronoi generation to respect annotations
    void apply_annotations_to_voronoi(
        VoronoiDiagram& diagram,
        const std::vector<FractureAnnotation>& annotations);

    // Check if impact is near weak point (guaranteed break)
    bool is_near_weak_point(
        const math::Vec3& impact_pos,
        const std::vector<FractureAnnotation>& annotations) const;
};

// Usage in fracture generation
std::vector<DebrisPiece> fracture_with_annotations(
    const TileInstance& tile,
    const ImpactData& impact)
{
    auto annotations = annotation_system.load_annotations(tile.mesh_path);

    // Check if impact is near artist-defined weak point
    if (annotation_system.is_near_weak_point(impact.position, annotations)) {
        // Use pre-defined fracture pattern
        return use_predefined_fracture(tile, annotations);
    }

    // Otherwise, procedural Voronoi with annotation guidance
    auto voronoi = generate_voronoi_with_guidance(tile, impact, annotations);
    return clip_and_create_fragments(voronoi);
}

} // namespace lore::world
```

### Blender Workflow

```python
# Blender Python script to annotate meshes
import bpy

# Add weak point at hinge
bpy.ops.object.empty_add(type='SPHERE', location=(0.5, 0, 1.5))
empty = bpy.context.object
empty.name = "WeakPoint_Hinge_Left"
empty["fracture_type"] = "weak_point"
empty["damage_threshold"] = 0.3
empty["affected_fragments"] = [0, 1, 2]

# Export to glTF with custom properties
bpy.ops.export_scene.gltf(
    filepath="CastleGate.glb",
    export_extras=True  # Include custom properties
)
```

---

## System 6: Structural Integrity and Collapse Simulation

### Problem
- Tiles float in midair after supports destroyed
- No progressive collapse (domino effect)
- Buildings should fall realistically

### Solution: Load-Bearing Analysis

```cpp
namespace lore::world {

struct StructuralNode {
    TileCoord coord;
    float load;                    // Weight supported (kg)
    float max_load_capacity;       // Before failure (kg)
    std::vector<TileCoord> supports;     // What this tile rests on
    std::vector<TileCoord> supported_by; // What supports this tile
    bool is_load_bearing = false;
};

class StructuralIntegritySystem {
public:
    // Analyze structure when tile is added/removed
    void recalculate_structural_graph(const TileCoord& changed_tile);

    // Find all tiles that will collapse if this tile breaks
    std::vector<TileCoord> find_dependent_tiles(const TileCoord& broken_tile);

    // Simulate progressive collapse (domino effect)
    void simulate_collapse(const TileCoord& initial_failure, float delta_time);

    // Check if tile is overloaded (will fail soon)
    bool is_overloaded(const TileCoord& tile) const;

private:
    // Structural graph (which tiles support which)
    std::unordered_map<TileCoord, StructuralNode> structural_graph_;

    // Breadth-first search to find all supported tiles
    void trace_supported_tiles(
        const TileCoord& root,
        std::vector<TileCoord>& out_tiles);

    // Calculate load on tile (weight of all tiles above)
    float calculate_load(const TileCoord& tile);
};

} // namespace lore::world
```

### Progressive Collapse Example

```cpp
// Destroy pillar in castle
destroy_tile({5, 5, 0});  // Ground floor pillar

// System detects collapse
auto dependent_tiles = structural_system.find_dependent_tiles({5, 5, 0});
// Returns: {5,5,1}, {5,5,2}, {5,5,3}, {4,5,3}, {6,5,3}...

// Simulate progressive collapse over 2 seconds
structural_system.simulate_collapse({5, 5, 0}, 2.0f);

// Each tile falls when support is lost:
// Frame 0.0s: Pillar destroyed
// Frame 0.1s: Tile {5,5,1} falls (loses support)
// Frame 0.2s: Tile {5,5,2} falls
// Frame 0.3s: Arch tiles {4,5,3} and {6,5,3} start cracking
// Frame 0.5s: Arch collapses
// Frame 1.0s: Upper walls fall
```

---

## Performance Targets

### Pre-Fracture System
- ✅ Zero runtime cost (load from disk)
- ✅ 5 variants per mesh = 5× visual variety
- ✅ <1ms to load fracture plan

### Directional Fracture
- ✅ 15-30 fragments typical (scalable)
- ✅ Stress-guided Voronoi: +2ms overhead
- ✅ Impact-aware fragment sizing: +0.5ms

### Multi-Material Layers
- ✅ 3 layers typical (stone + wood + plaster)
- ✅ Each layer fractures independently
- ✅ +5ms per extra layer

### Progressive Damage
- ✅ Crack propagation: 0.1ms per crack per frame
- ✅ 10-20 cracks typical before collapse
- ✅ Decal rendering: existing deferred renderer

### Structural Integrity
- ✅ BFS graph traversal: <1ms for 100 tiles
- ✅ Collapse simulation: 2-5 tiles per frame
- ✅ Total scene collapse: 2-3 seconds

---

## Implementation Priority

### Phase 1: Pre-Fracture Cache (Week 1)
1. Offline fracture tool (`lore-fracture-tool`)
2. `.fracture` file format
3. Runtime loader with fallback to Voronoi

### Phase 2: Directional Fracture (Week 2)
1. `ImpactData` struct and impact types
2. Stress-guided point generation
3. Fragment size gradient

### Phase 3: Progressive Damage (Week 3)
1. Crack path generation and rendering
2. Crack propagation update loop
3. Damage state machine

### Phase 4: Multi-Material (Week 4)
1. `MaterialLayer` system
2. Layered fracture algorithm
3. Per-layer particle effects

### Phase 5: Structural Integrity (Week 5)
1. Load-bearing graph construction
2. Dependency analysis
3. Progressive collapse simulation

### Phase 6: Artist Tools (Week 6)
1. Blender/Maya annotation scripts
2. glTF metadata export
3. Annotation-guided fracture

---

## Conclusion

This advanced system provides:
- ✅ **Instant destruction** (pre-fractured meshes)
- ✅ **Realistic fracture** (impact-aware Voronoi)
- ✅ **Material variety** (stone ≠ wood ≠ glass)
- ✅ **Progressive damage** (cracks before collapse)
- ✅ **Artist control** (predictable gameplay)
- ✅ **Structural realism** (progressive collapse)

**Result:** Destruction quality matching or exceeding Control, Teardown, and Red Faction Guerrilla.
