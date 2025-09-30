# World Materials and Structural Physics
## Material Properties for Every Object - Load Bearing, Breakage, Realistic Physics

**Critical Principles:**
- **Material matters**: Wood ≠ steel ≠ concrete. Everything has real properties.
- **Gravity matters**: Structural loads, falling objects, projectile trajectories.
- **Physics matters**: No abstract "health bars" for walls - calculate actual stress and fracture.

---

## Material Properties (From Existing `physics::Material`)

Already have foundation:
```cpp
struct Material {
    float density;                    // kg/m³
    float friction;                   // Surface friction coefficient
    float restitution;                // Bounciness (0-1)
    float hardness;                   // Mohs scale (0-10)
    float thermal_conductivity;       // W/(m·K)
    float electrical_conductivity;    // S/m
};
```

**Need to extend for structural physics:**

```cpp
struct StructuralMaterial : public physics::Material {
    // Strength Properties
    float tensile_strength_pa;        // Pascals (pulling force before break)
    float compressive_strength_pa;    // Pascals (crushing force before break)
    float shear_strength_pa;          // Pascals (sliding force before break)
    float yield_strength_pa;          // Permanent deformation point

    // Elastic Properties
    float youngs_modulus_pa;          // Stiffness (stress/strain ratio)
    float poissons_ratio;             // Transverse strain (0-0.5)
    float bulk_modulus_pa;            // Resistance to compression

    // Fracture Properties
    float fracture_toughness;         // Resistance to crack propagation
    float critical_stress_intensity;  // Stress at crack tip to propagate
    bool is_brittle;                  // Brittle (shatters) vs ductile (bends)

    // Load Bearing
    float max_stress_pa;              // Maximum stress before failure
    float fatigue_limit_pa;           // Stress that can be sustained indefinitely

    /**
     * @brief Material presets for common materials
     */
    static StructuralMaterial create_wood() {
        return {
            .density = 600.0f,              // kg/m³ (pine)
            .hardness = 2.0f,                // Soft wood
            .tensile_strength_pa = 40e6f,    // 40 MPa
            .compressive_strength_pa = 30e6f, // 30 MPa
            .shear_strength_pa = 5e6f,       // 5 MPa
            .yield_strength_pa = 25e6f,
            .youngs_modulus_pa = 11e9f,      // 11 GPa
            .poissons_ratio = 0.3f,
            .fracture_toughness = 0.5f,
            .is_brittle = false              // Wood bends before breaking
        };
    }

    static StructuralMaterial create_concrete() {
        return {
            .density = 2400.0f,              // kg/m³
            .hardness = 7.0f,
            .tensile_strength_pa = 3e6f,     // Weak in tension
            .compressive_strength_pa = 30e6f, // Strong in compression
            .shear_strength_pa = 10e6f,
            .youngs_modulus_pa = 30e9f,      // 30 GPa
            .poissons_ratio = 0.2f,
            .fracture_toughness = 0.2f,
            .is_brittle = true               // Concrete shatters
        };
    }

    static StructuralMaterial create_steel() {
        return {
            .density = 7850.0f,              // kg/m³
            .hardness = 4.5f,
            .tensile_strength_pa = 400e6f,   // 400 MPa
            .compressive_strength_pa = 400e6f,
            .shear_strength_pa = 250e6f,
            .youngs_modulus_pa = 200e9f,     // 200 GPa
            .poissons_ratio = 0.3f,
            .fracture_toughness = 50.0f,     // Very tough
            .is_brittle = false              // Steel deforms before breaking
        };
    }

    static StructuralMaterial create_glass() {
        return {
            .density = 2500.0f,
            .hardness = 6.0f,
            .tensile_strength_pa = 50e6f,
            .compressive_strength_pa = 1000e6f, // Strong in compression
            .shear_strength_pa = 35e6f,
            .youngs_modulus_pa = 70e9f,
            .fracture_toughness = 0.7f,
            .is_brittle = true               // Glass shatters spectacularly
        };
    }

    static StructuralMaterial create_brick() {
        return {
            .density = 1800.0f,
            .hardness = 4.0f,
            .tensile_strength_pa = 2e6f,     // Weak in tension
            .compressive_strength_pa = 20e6f,
            .shear_strength_pa = 8e6f,
            .youngs_modulus_pa = 15e9f,
            .is_brittle = true
        };
    }
};
```

---

## World Mesh Material Component

**Every renderable object needs material metadata:**

```cpp
struct WorldMeshMaterialComponent {
    /// Material for each submesh/polygon group
    std::vector<StructuralMaterial> materials;

    /// Mass per vertex (for load bearing calculations)
    std::vector<float> vertex_loads;

    /// Stress state (calculated per frame)
    struct StressState {
        float tensile_stress_pa = 0.0f;      // Pulling force
        float compressive_stress_pa = 0.0f;  // Crushing force
        float shear_stress_pa = 0.0f;        // Sliding force
        float von_mises_stress_pa = 0.0f;    // Combined stress
        bool is_yielding = false;            // Permanent deformation?
        bool is_fractured = false;           // Has it broken?
    };

    std::vector<StressState> vertex_stress;  // Per-vertex stress

    /// Load bearing connections (which vertices support which)
    struct LoadBearingEdge {
        uint32_t vertex_a;
        uint32_t vertex_b;
        float load_capacity_n;               // Newtons it can support
        float current_load_n = 0.0f;
        bool is_critical = false;            // Structural failure if breaks?
    };

    std::vector<LoadBearingEdge> load_bearing_edges;

    /**
     * @brief Calculate gravitational load on structure
     *
     * This runs on GPU (Task 8: GPU Compute Systems)
     */
    void calculate_loads(const math::Vec3& gravity) {
        // For each vertex, sum up mass of everything it supports
        // Propagate loads down through load_bearing_edges
        // This is a graph traversal problem - perfect for GPU!
    }

    /**
     * @brief Check if structure exceeds stress limits
     *
     * @return List of failed vertices/edges
     */
    std::vector<uint32_t> check_structural_failure() {
        std::vector<uint32_t> failed_vertices;

        for (size_t i = 0; i < vertex_stress.size(); ++i) {
            const auto& stress = vertex_stress[i];
            const auto& material = materials[i % materials.size()];

            // Check if stress exceeds material strength
            if (stress.tensile_stress_pa > material.tensile_strength_pa ||
                stress.compressive_stress_pa > material.compressive_strength_pa ||
                stress.von_mises_stress_pa > material.max_stress_pa) {

                failed_vertices.push_back(i);
            }
        }

        return failed_vertices;
    }

    /**
     * @brief Apply projectile impact and calculate damage
     *
     * Uses ballistics energy to calculate stress
     */
    void apply_impact(
        uint32_t vertex_index,
        const math::Vec3& impact_direction,
        float kinetic_energy_j
    ) {
        // Convert kinetic energy to stress (force over area)
        // Simplified: energy / (vertex area * material toughness)
        const auto& material = materials[vertex_index % materials.size()];

        // Calculate impact stress (Pa = N/m²)
        float impact_area_m2 = 0.01f; // Approximate vertex area
        float impact_force_n = kinetic_energy_j / 0.1f; // Energy over distance
        float impact_stress_pa = impact_force_n / impact_area_m2;

        // Apply stress
        auto& stress = vertex_stress[vertex_index];

        // Determine stress type based on impact direction
        float normal_component = math::dot(impact_direction, get_vertex_normal(vertex_index));

        if (normal_component > 0) {
            // Compressive stress (pushing in)
            stress.compressive_stress_pa += impact_stress_pa;
        } else {
            // Tensile stress (pulling)
            stress.tensile_stress_pa += impact_stress_pa;
        }

        // Check for fracture
        if (stress.tensile_stress_pa > material.tensile_strength_pa ||
            stress.compressive_stress_pa > material.compressive_strength_pa) {

            stress.is_fractured = true;

            // Brittle materials shatter (propagate fracture)
            if (material.is_brittle) {
                propagate_fracture(vertex_index);
            }
        }
    }

    /**
     * @brief Fracture propagation (cracks spread through brittle materials)
     */
    void propagate_fracture(uint32_t origin_vertex) {
        // Find connected vertices
        // Propagate crack based on stress intensity factor
        // GPU compute shader for this! (Task 8)
    }
};
```

---

## Structural Integrity System

**System that calculates loads, stress, and handles collapse:**

```cpp
class StructuralIntegritySystem : public ecs::System {
public:
    void init(ecs::World& world) override {
        // Initialize GPU compute shaders for load calculation
    }

    void update(ecs::World& world, float delta_time) override {
        // For each entity with WorldMeshMaterialComponent:

        // 1. Calculate gravitational loads (GPU)
        //    - Sum up mass supported by each vertex
        //    - Propagate down through load-bearing graph

        // 2. Calculate stress from external forces
        //    - Projectile impacts
        //    - Explosions
        //    - Vehicle collisions

        // 3. Check for structural failure
        //    - Vertices exceeding material strength
        //    - Critical load-bearing edges broken

        // 4. Handle collapse
        //    - Separate fractured sections
        //    - Apply gravity to unsupported sections
        //    - Create debris entities

        // 5. Fracture propagation (brittle materials)
        //    - Cracks spread through glass, concrete
        //    - Ductile materials (steel) deform instead
    }

    /**
     * @brief Calculate load distribution through structure
     *
     * This is a HUGE calculation - perfect for GPU (Task 8)
     */
    void calculate_structural_loads_gpu(ecs::World& world) {
        // Dispatch compute shader:
        // - Input: Mesh vertices, edges, materials, gravity
        // - Output: Per-vertex loads and stress
        //
        // Parallel BFS through load-bearing graph
        // Each workgroup handles a sub-structure
    }

private:
    VkPipeline load_calculation_pipeline_;
    VkPipeline fracture_propagation_pipeline_;
};
```

---

## Gravity and Environmental Forces

**All forces affect structures:**

```cpp
struct EnvironmentalForcesComponent {
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};  // m/s²

    math::Vec3 wind_velocity{0.0f};          // m/s
    float wind_pressure_pa = 0.0f;           // Wind load on surfaces

    float ambient_temperature_k = 293.15f;   // Affects material properties
    float ambient_pressure_pa = 101325.0f;   // 1 atmosphere

    /**
     * @brief Calculate wind load on surface
     */
    float calculate_wind_load(float surface_area_m2, const math::Vec3& surface_normal) const {
        // Wind pressure = 0.5 * air_density * velocity² * drag_coefficient
        float air_density = 1.225f; // kg/m³ at sea level
        float drag_coefficient = 1.28f; // Flat plate

        float wind_speed = math::length(wind_velocity);
        float dynamic_pressure = 0.5f * air_density * wind_speed * wind_speed;

        // Project wind onto surface
        float normal_component = std::abs(math::dot(
            math::normalize(wind_velocity),
            surface_normal
        ));

        return dynamic_pressure * drag_coefficient * surface_area_m2 * normal_component;
    }

    /**
     * @brief Gravity affects everything
     */
    float calculate_gravitational_force(float mass_kg) const {
        return mass_kg * math::length(gravity);
    }
};
```

---

## Example: Building Collapse

```cpp
// Building has WorldMeshMaterialComponent with concrete walls
auto& building_materials = world.get_component<WorldMeshMaterialComponent>(building_entity);

// Explosion nearby - apply pressure wave
math::Vec3 explosion_position{10.0f, 0.0f, 5.0f};
float explosion_energy_j = 1000000.0f; // 1 MJ

// Calculate which vertices are affected
for (uint32_t v = 0; v < building_materials.vertex_stress.size(); ++v) {
    math::Vec3 vertex_pos = get_vertex_position(building_entity, v);
    float distance = math::length(vertex_pos - explosion_position);

    // Inverse square law for pressure
    float pressure_pa = explosion_energy_j / (4.0f * M_PI * distance * distance);

    // Apply pressure as stress
    math::Vec3 direction = math::normalize(vertex_pos - explosion_position);
    building_materials.apply_impact(v, direction, pressure_pa * 0.1f);
}

// Check for failures
auto failed_vertices = building_materials.check_structural_failure();

if (!failed_vertices.empty()) {
    // Concrete is brittle - cracks propagate
    for (uint32_t v : failed_vertices) {
        building_materials.propagate_fracture(v);
    }

    // Check if load-bearing walls failed
    bool critical_failure = check_critical_supports(building_materials, failed_vertices);

    if (critical_failure) {
        // Building collapses!
        trigger_structural_collapse(building_entity);
    }
}
```

---

## GPU Compute Integration (Task 8)

**These calculations are PERFECT for GPU:**

1. **Load Distribution**: Parallel graph traversal
2. **Stress Calculation**: Per-vertex parallel computation
3. **Fracture Propagation**: Cellular automaton on GPU
4. **Collision Detection**: Spatial hashing on GPU

```wgsl
// GPU Compute Shader for Load Distribution
@group(0) @binding(0) var<storage, read> vertices: array<vec3<f32>>;
@group(0) @binding(1) var<storage, read> masses: array<f32>;
@group(0) @binding(2) var<storage, read> edges: array<LoadBearingEdge>;
@group(0) @binding(3) var<storage, read_write> loads: array<f32>;
@group(0) @binding(4) var<storage, read_write> stress: array<StressState>;

@compute @workgroup_size(256)
fn calculate_structural_loads(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let vertex_id = global_id.x;
    if (vertex_id >= arrayLength(&vertices)) { return; }

    // Calculate gravitational load
    let gravity_force = masses[vertex_id] * push_constants.gravity_magnitude;

    // Propagate load to supporting vertices
    // (This is simplified - real version uses iterative solver)
    var total_load = gravity_force;

    for (var i = 0u; i < arrayLength(&edges); i++) {
        let edge = edges[i];
        if (edge.vertex_b == vertex_id) {
            // This vertex supports vertex_a
            total_load += loads[edge.vertex_a];
        }
    }

    loads[vertex_id] = total_load;

    // Calculate stress (load / cross-sectional area)
    let vertex_area = 0.01; // m² (approximate)
    let stress_pa = total_load / vertex_area;

    stress[vertex_id].compressive_stress_pa = stress_pa;
}
```

---

## Summary

**Material matters:**
- ✅ Every object has StructuralMaterial with real properties
- ✅ Wood, steel, concrete, glass all behave differently
- ✅ Brittle materials shatter, ductile materials deform

**Gravity matters:**
- ✅ Structures bear gravitational loads
- ✅ Load distribution through load-bearing graph
- ✅ Failure when stress exceeds material strength

**Physics matters:**
- ✅ NO abstract "building health"
- ✅ Calculate actual stress and fracture
- ✅ Projectiles/explosions apply real forces
- ✅ Structural collapse from failed load-bearing elements

**GPU acceleration (Task 8):**
- ✅ Load distribution computed in parallel
- ✅ Stress calculations on thousands of vertices
- ✅ Fracture propagation as cellular automaton
- ✅ Real-time structural physics for entire world

**This is what realistic physics looks like!**