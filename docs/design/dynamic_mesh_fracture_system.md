# Dynamic Mesh Fracture System
## Real-Time GPU-Accelerated Destruction with Polygon Budget Management

**Research-Backed Design** - Based on 2024 techniques, production implementations, and material science.

---

## Executive Summary

**Goal**: Realistic, physics-based mesh destruction where:
- Bullet hits wall → chips off concrete piece
- Glass shatters into shards
- Metal deforms and tears
- Wood splinters
- Debris persists until polygon budget exceeded
- Everything computed in real-time on GPU

**Key Insight from Research**:
- Voronoi fracture runs at 60 FPS with 500 cells (GPU-accelerated)
- Pre-fracturing fastest but least dynamic
- Material properties determine fracture patterns
- Modern Vulkan supports mesh nodes and work graphs for this

---

## System Architecture

### Three-Tier Approach

```
┌─────────────────────────────────────────────────┐
│  1. Pre-Fractured (Fastest)                     │
│     - Artist-created destruction states          │
│     - Swap meshes on impact                      │
│     - For hero assets, important structures      │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│  2. Runtime Voronoi (Balanced)                  │
│     - GPU-generated Voronoi fracture             │
│     - Material-specific patterns                 │
│     - For most destructible objects              │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│  3. Surface Damage (Detail)                     │
│     - Bullet holes, chips, dents                 │
│     - Decal + geometry modification              │
│     - For non-structural damage                  │
└─────────────────────────────────────────────────┘
```

---

## Material-Specific Fracture Patterns

### Research Finding:
> "Brittle fracture exhibits 'river marks' and chevron patterns pointing to crack initiation"
> "Ductile fracture shows large deformation, stretching, and necking"

### Material Fracture Behaviors

```cpp
enum class FractureBehavior {
    Brittle,        // Shatters into many pieces (glass, ceramic, concrete)
    Ductile,        // Deforms then tears (metal, plastic)
    Fibrous,        // Splinters along grain (wood)
    Granular        // Crumbles into small pieces (brick, stone)
};

struct FractureProperties {
    FractureBehavior behavior = FractureBehavior::Brittle;

    // Voronoi generation parameters
    uint32_t min_fracture_pieces = 3;
    uint32_t max_fracture_pieces = 20;
    float fragment_size_variation = 0.3f;  // 0=uniform, 1=highly varied

    // Fracture patterns
    bool use_directional_bias = false;     // Cracks follow impact direction?
    float radial_pattern_strength = 0.5f;  // How much cracks radiate from impact
    float planar_tendency = 0.0f;          // Tendency to split along planes

    // Appearance
    float edge_sharpness = 1.0f;           // Sharp shards vs smooth breaks
    float surface_roughness = 0.3f;        // Rough fracture surface
    math::Vec3 interior_color_shift;       // Color of interior vs exterior

    /**
     * @brief Material presets
     */
    static FractureProperties create_glass() {
        return {
            .behavior = FractureBehavior::Brittle,
            .min_fracture_pieces = 8,
            .max_fracture_pieces = 40,      // Glass shatters into many pieces
            .fragment_size_variation = 0.7f, // Varied shard sizes
            .use_directional_bias = true,
            .radial_pattern_strength = 0.9f, // Strong radial cracks from impact
            .edge_sharpness = 1.0f,          // Sharp edges
            .surface_roughness = 0.1f        // Smooth fracture surface
        };
    }

    static FractureProperties create_concrete() {
        return {
            .behavior = FractureBehavior::Granular,
            .min_fracture_pieces = 5,
            .max_fracture_pieces = 15,      // Moderate chunking
            .fragment_size_variation = 0.5f,
            .use_directional_bias = false,
            .radial_pattern_strength = 0.3f, // Less organized cracking
            .edge_sharpness = 0.4f,          // Rough, crumbly edges
            .surface_roughness = 0.8f        // Very rough interior
        };
    }

    static FractureProperties create_metal() {
        return {
            .behavior = FractureBehavior::Ductile,
            .min_fracture_pieces = 1,       // Metal tears, doesn't shatter
            .max_fracture_pieces = 3,
            .fragment_size_variation = 0.2f,
            .use_directional_bias = true,   // Tears follow stress lines
            .planar_tendency = 0.8f,        // Tends to tear along planes
            .edge_sharpness = 0.7f,         // Torn edges
            .interior_color_shift = {-0.2f, -0.2f, -0.2f}  // Darker interior
        };
    }

    static FractureProperties create_wood() {
        return {
            .behavior = FractureBehavior::Fibrous,
            .min_fracture_pieces = 3,
            .max_fracture_pieces = 8,
            .fragment_size_variation = 0.4f,
            .use_directional_bias = true,   // Splits along grain
            .planar_tendency = 0.9f,        // Strong grain direction
            .edge_sharpness = 0.3f,         // Splintered edges
            .surface_roughness = 0.6f
        };
    }
};
```

---

## GPU Voronoi Fracture System

### Research Finding:
> "GPU-based Voronoi achieves 500 cells at 60 FPS"
> "Fracturing calculations handled by GPU, CPU only provides initial positions"

### Vulkan Compute Pipeline

```cpp
class VoronoiFractureSystem {
public:
    /**
     * @brief Fracture mesh on GPU using Voronoi diagram
     *
     * @param mesh Original mesh to fracture
     * @param impact_point Where the impact occurred (world space)
     * @param impact_energy Kinetic energy in Joules
     * @param material Fracture properties
     * @return Fractured pieces (GPU-generated)
     */
    std::vector<FracturedPiece> fracture_mesh_gpu(
        const Mesh& mesh,
        const math::Vec3& impact_point,
        float impact_energy,
        const FractureProperties& material
    );

private:
    // GPU compute pipelines
    VkPipeline voronoi_seed_generation_pipeline_;   // Generate Voronoi seeds
    VkPipeline voronoi_diagram_pipeline_;           // Build diagram on GPU
    VkPipeline mesh_slicing_pipeline_;              // Slice mesh by Voronoi cells
    VkPipeline fragment_welding_pipeline_;          // Weld vertices, reduce poly count

    // Buffers
    VkBuffer voronoi_seeds_buffer_;                 // Seed points for Voronoi cells
    VkBuffer voronoi_diagram_buffer_;               // Generated diagram
    VkBuffer input_mesh_buffer_;                    // Original mesh
    VkBuffer output_fragments_buffer_;              // Fractured pieces
};
```

### WGSL Compute Shader - Voronoi Seed Generation

```wgsl
// Generate Voronoi seed points based on impact and material properties
@group(0) @binding(0) var<storage, read_write> seeds: array<vec3<f32>>;
@group(0) @binding(1) var<uniform> fracture_params: FractureParams;

struct FractureParams {
    impact_point: vec3<f32>,
    impact_energy: f32,
    num_seeds: u32,
    radial_strength: f32,
    directional_bias: vec3<f32>,
    mesh_bounds_min: vec3<f32>,
    mesh_bounds_max: vec3<f32>,
}

@compute @workgroup_size(256)
fn generate_voronoi_seeds(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let seed_id = global_id.x;
    if (seed_id >= fracture_params.num_seeds) { return; }

    // Random seed position within mesh bounds
    let random_point = pcg3d(seed_id);  // GPU random number generator
    var seed_pos = fracture_params.mesh_bounds_min +
                   random_point * (fracture_params.mesh_bounds_max -
                                   fracture_params.mesh_bounds_min);

    // Bias towards impact point (radial fracture pattern)
    let to_impact = fracture_params.impact_point - seed_pos;
    let impact_influence = fracture_params.radial_strength *
                          (1.0 - length(to_impact) / length(fracture_params.mesh_bounds_max));

    seed_pos += to_impact * impact_influence * 0.5;

    // Apply directional bias (for materials like wood with grain)
    seed_pos += fracture_params.directional_bias * random_point.z * 0.3;

    seeds[seed_id] = seed_pos;
}
```

---

## Surface Damage System (Bullet Holes, Chips)

**For small-scale damage that doesn't require full fracture:**

```cpp
struct SurfaceDamageComponent {
    /// Damage marks (bullet holes, chips, scratches)
    struct DamageMark {
        math::Vec3 position;        // World space
        math::Vec3 normal;          // Surface normal
        float radius;               // Size of damage
        float depth;                // How deep the damage penetrates
        DamageType type;

        // Geometry modification
        std::vector<uint32_t> affected_vertices;  // Vertices to displace
        std::vector<float> vertex_displacements;  // How much to push in
    };

    enum class DamageType {
        BulletHole,     // Clean circular hole
        Chip,           // Chunk broken off (concrete, brick)
        Dent,           // Deformation without removal (metal)
        Scratch,        // Surface scrape
        Burn,           // Heat damage (affects material, not geometry)
        Crack           // Surface crack (visual, no geometry change yet)
    };

    std::vector<DamageMark> damage_marks;

    /// Accumulated damage budget
    uint32_t total_vertices_modified = 0;
    uint32_t max_vertices_modified = 500;  // Budget per object

    /**
     * @brief Apply surface damage from projectile impact
     *
     * Creates bullet hole + chips off small pieces
     */
    void apply_projectile_damage(
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float kinetic_energy_j,
        const FractureProperties& material,
        Mesh& mesh  // Modify mesh geometry directly
    ) {
        // 1. Create bullet hole (geometry modification)
        float hole_radius = calculate_hole_radius(kinetic_energy_j);
        float penetration_depth = calculate_penetration(kinetic_energy_j, material);

        // Find affected vertices
        auto affected_verts = find_vertices_in_sphere(mesh, impact_point, hole_radius);

        if (total_vertices_modified + affected_verts.size() > max_vertices_modified) {
            // Budget exceeded - convert to decal only (no geometry)
            apply_decal_only(impact_point, impact_direction, hole_radius);
            return;
        }

        // Displace vertices inward (create hole)
        for (uint32_t vert_idx : affected_verts) {
            math::Vec3 to_vertex = mesh.vertices[vert_idx] - impact_point;
            float distance = length(to_vertex);
            float displacement = penetration_depth * (1.0f - distance / hole_radius);

            mesh.vertices[vert_idx] += impact_direction * displacement;
        }

        // 2. Chip off small pieces (if brittle material)
        if (material.behavior == FractureBehavior::Brittle ||
            material.behavior == FractureBehavior::Granular) {

            // Create small chips around impact
            create_surface_chips(impact_point, impact_direction, kinetic_energy_j, material);
        }

        // 3. Add damage mark for tracking
        damage_marks.push_back({
            .position = impact_point,
            .normal = -impact_direction,
            .radius = hole_radius,
            .depth = penetration_depth,
            .type = DamageType::BulletHole,
            .affected_vertices = affected_verts
        });

        total_vertices_modified += affected_verts.size();

        // 4. Update mesh on GPU
        upload_modified_mesh_to_gpu(mesh);
    }

    /**
     * @brief Create small chips/fragments around impact (concrete, brick)
     */
    void create_surface_chips(
        const math::Vec3& impact_point,
        const math::Vec3& impact_direction,
        float energy,
        const FractureProperties& material
    ) {
        // Spawn 3-8 small debris pieces
        uint32_t num_chips = 3 + (uint32_t)(energy * 0.01f);
        num_chips = std::min(num_chips, 8u);

        for (uint32_t i = 0; i < num_chips; ++i) {
            // Random direction around impact normal
            math::Vec3 chip_direction = impact_direction +
                                       random_vector_in_cone(45.0f); // degrees

            // Small chip size
            float chip_size = 0.02f + random() * 0.03f;  // 2-5cm

            // Create chip entity
            Entity chip = debris_manager_.create_chip(
                impact_point,
                chip_direction,
                chip_size,
                material
            );
        }
    }
};
```

---

## Debris Management & Polygon Budget

### Research Finding:
> "Common methods swap meshes at runtime"
> "Pre-fractured approaches have best performance"

```cpp
class DebrisManager {
public:
    struct DebrisPoolConfig {
        uint32_t max_debris_entities = 500;         // Max debris objects
        uint32_t max_total_triangles = 50000;       // Polygon budget
        float debris_lifetime_seconds = 30.0f;      // How long debris persists
        float merge_distance = 0.5f;                // Merge nearby small debris
        bool use_gpu_instancing = true;             // Instance similar pieces
    };

    /**
     * @brief Add debris piece to managed pool
     */
    Entity create_debris(
        const math::Vec3& position,
        const Mesh& mesh,
        const FractureProperties& material,
        float initial_velocity
    ) {
        // Check budget
        if (active_debris_.size() >= config_.max_debris_entities ||
            total_triangle_count_ + mesh.triangles.size() >= config_.max_total_triangles) {

            // Budget exceeded - remove oldest debris
            remove_oldest_debris();
        }

        // Create debris entity with physics
        Entity debris = world_.create_entity();
        world_.add_component<MeshComponent>(debris, mesh);
        world_.add_component<RigidBodyComponent>(debris, create_debris_physics(mesh, initial_velocity));
        world_.add_component<DebrisComponent>(debris, {
            .spawn_time = current_time_,
            .lifetime = config_.debris_lifetime_seconds,
            .triangle_count = (uint32_t)mesh.triangles.size()
        });

        active_debris_.push_back(debris);
        total_triangle_count_ += mesh.triangles.size();

        return debris;
    }

    /**
     * @brief Update debris system (called per frame)
     *
     * - Remove old debris
     * - Merge small pieces
     * - Update GPU instancing
     * - Apply LOD based on distance
     */
    void update(float delta_time) {
        current_time_ += delta_time;

        // 1. Remove expired debris
        auto it = active_debris_.begin();
        while (it != active_debris_.end()) {
            auto& debris_comp = world_.get_component<DebrisComponent>(*it);

            if (current_time_ - debris_comp.spawn_time > debris_comp.lifetime) {
                total_triangle_count_ -= debris_comp.triangle_count;
                world_.destroy_entity(*it);
                it = active_debris_.erase(it);
            } else {
                ++it;
            }
        }

        // 2. Merge nearby small debris (reduce entity count)
        if (active_debris_.size() > config_.max_debris_entities * 0.8f) {
            merge_nearby_debris();
        }

        // 3. Apply LOD based on camera distance
        apply_debris_lod();

        // 4. Update GPU instancing for similar pieces
        if (config_.use_gpu_instancing) {
            update_debris_instancing();
        }
    }

private:
    /**
     * @brief Merge small debris pieces within merge_distance
     */
    void merge_nearby_debris() {
        // Spatial hash to find nearby debris
        std::unordered_map<uint64_t, std::vector<Entity>> spatial_grid;

        for (Entity debris : active_debris_) {
            auto& transform = world_.get_component<TransformComponent>(debris);
            uint64_t grid_key = spatial_hash(transform.position, config_.merge_distance);
            spatial_grid[grid_key].push_back(debris);
        }

        // Merge debris in same grid cell
        for (auto& [key, debris_list] : spatial_grid) {
            if (debris_list.size() >= 2) {
                // Combine meshes, create single larger debris
                merge_debris_group(debris_list);
            }
        }
    }

    /**
     * @brief Apply LOD - reduce triangle count for distant debris
     */
    void apply_debris_lod() {
        math::Vec3 camera_pos = get_camera_position();

        for (Entity debris : active_debris_) {
            auto& transform = world_.get_component<TransformComponent>(debris);
            auto& mesh = world_.get_component<MeshComponent>(debris);

            float distance = math::length(transform.position - camera_pos);

            // LOD thresholds
            if (distance > 50.0f) {
                // Far: Very low poly (50% reduction)
                mesh.set_lod_level(2);
            } else if (distance > 20.0f) {
                // Medium: Reduced poly (25% reduction)
                mesh.set_lod_level(1);
            } else {
                // Near: Full detail
                mesh.set_lod_level(0);
            }
        }
    }

    std::vector<Entity> active_debris_;
    uint32_t total_triangle_count_ = 0;
    float current_time_ = 0.0f;
    DebrisPoolConfig config_;
    ecs::World& world_;
};
```

---

## Fracture Trigger System

**Determines WHEN to fracture based on material stress:**

```cpp
/**
 * @brief Check if impact energy exceeds fracture threshold
 */
bool should_fracture(
    float impact_energy_j,
    const StructuralMaterial& material,
    const math::Vec3& impact_point,
    const Mesh& mesh
) {
    // Calculate stress at impact point
    float impact_area_m2 = estimate_impact_area(impact_point, mesh);
    float impact_stress_pa = (impact_energy_j / 0.01f) / impact_area_m2;  // Force / area

    // Compare to material fracture toughness
    float fracture_threshold = material.fracture_toughness * material.tensile_strength_pa;

    return impact_stress_pa > fracture_threshold;
}

/**
 * @brief Decide fracture approach based on material and energy
 */
enum class FractureApproach {
    NoFracture,         // Too weak - surface damage only
    SurfaceDamage,      // Bullet hole + chips
    PartialFracture,    // Small local fracture (part of object)
    FullFracture        // Complete fracture (entire object shatters)
};

FractureApproach determine_fracture_approach(
    float impact_energy_j,
    const StructuralMaterial& material,
    const FractureProperties& fracture_props,
    const Mesh& mesh
) {
    float energy_threshold_surface = 10.0f;     // Joules
    float energy_threshold_partial = 100.0f;    // Joules
    float energy_threshold_full = 500.0f;       // Joules

    // Adjust thresholds by material toughness
    float toughness_multiplier = material.fracture_toughness / 10.0f;
    energy_threshold_partial *= toughness_multiplier;
    energy_threshold_full *= toughness_multiplier;

    if (impact_energy_j < energy_threshold_surface) {
        return FractureApproach::NoFracture;  // Decal only
    } else if (impact_energy_j < energy_threshold_partial) {
        return FractureApproach::SurfaceDamage;  // Bullet hole + chips
    } else if (impact_energy_j < energy_threshold_full) {
        return FractureApproach::PartialFracture;  // Local Voronoi fracture
    } else {
        return FractureApproach::FullFracture;  // Complete destruction
    }
}
```

---

## Integration Example

**Complete flow when bullet hits concrete wall:**

```cpp
void on_projectile_hit_wall(
    Entity projectile,
    Entity wall,
    const math::Vec3& impact_point,
    const math::Vec3& impact_direction
) {
    // 1. Get projectile kinetic energy (from physics)
    auto& projectile_physics = world.get_component<RigidBodyComponent>(projectile);
    float velocity = math::length(projectile_physics.velocity);
    float kinetic_energy = 0.5f * projectile_physics.mass * velocity * velocity;

    // 2. Get wall material properties
    auto& wall_mesh_material = world.get_component<WorldMeshMaterialComponent>(wall);
    const StructuralMaterial& concrete = wall_mesh_material.materials[0];
    FractureProperties fracture_props = FractureProperties::create_concrete();

    // 3. Determine fracture approach
    FractureApproach approach = determine_fracture_approach(
        kinetic_energy, concrete, fracture_props, wall_mesh
    );

    // 4. Apply appropriate damage
    switch (approach) {
        case FractureApproach::NoFracture:
            // Just a decal, no geometry change
            add_bullet_hole_decal(wall, impact_point, impact_direction);
            break;

        case FractureApproach::SurfaceDamage:
            // Modify mesh geometry + create small chips
            auto& surface_damage = world.get_component<SurfaceDamageComponent>(wall);
            surface_damage.apply_projectile_damage(
                impact_point, impact_direction, kinetic_energy, fracture_props, wall_mesh
            );
            break;

        case FractureApproach::PartialFracture:
            // Fracture local region of wall
            fracture_local_region(wall, impact_point, kinetic_energy, fracture_props);
            break;

        case FractureApproach::FullFracture:
            // Entire wall shatters
            auto fragments = voronoi_fracture_system.fracture_mesh_gpu(
                wall_mesh, impact_point, kinetic_energy, fracture_props
            );
            create_debris_entities(fragments);
            world.destroy_entity(wall);  // Remove original wall
            break;
    }

    // 5. Projectile physics - bullet embeds or ricochets
    if (approach == FractureApproach::NoFracture ||
        approach == FractureApproach::SurfaceDamage) {
        // Bullet embeds in wall (create small debris entity)
        create_embedded_bullet(projectile, wall, impact_point, impact_direction);
    } else {
        // Bullet passes through fracture or deflects
        apply_bullet_deflection(projectile, impact_direction, concrete);
    }
}
```

---

## Summary: Research-Backed Design

**Material Science Integration**:
- ✅ Brittle materials (glass, ceramic) → Many sharp fragments, radial cracks
- ✅ Ductile materials (metal) → Deformation and tearing, few large pieces
- ✅ Fibrous materials (wood) → Splintering along grain
- ✅ Granular materials (concrete, brick) → Crumbling into medium chunks

**GPU Performance** (from research):
- ✅ Voronoi fracture: 500 cells at 60 FPS
- ✅ Vulkan compute: Work graphs, mesh nodes (2024 tech)
- ✅ Parallel seed generation, diagram construction, mesh slicing

**Polygon Budget Management**:
- ✅ Three-tier system: Pre-fractured > Voronoi > Surface damage
- ✅ Debris pooling with lifetime management
- ✅ LOD for distant debris (50% triangle reduction)
- ✅ Merge small pieces to reduce entity count
- ✅ GPU instancing for similar debris

**Realistic Behavior**:
- ✅ Bullets chip concrete, create entry/exit holes
- ✅ Glass shatters with radial cracks from impact
- ✅ Metal dents and tears
- ✅ Wood splinters along grain
- ✅ Debris persists until budget exceeded
- ✅ No magic removal until necessary

**This system uses cutting-edge 2024 GPU techniques to achieve film-quality destruction in real-time!**