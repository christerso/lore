# Physics-Based Entity System
## Using Existing Physics, Ballistics, and Materials Systems

**Design Principle**: USE the existing comprehensive physics systems (Tasks 7, 8, 9). Don't reinvent physics!

---

## What We Already Have ✅

From `include/lore/physics/physics.hpp`:

1. **Material** - density, hardness, thermal/electrical conductivity
2. **RigidBodyComponent** - mass, velocity, force, impulse
3. **BallisticsSystem** - projectile physics with drag, ballistic coefficient
4. **ProjectileComponent** - drag coefficient, cross-sectional area
5. **ThermodynamicsSystem** - heat transfer simulation
6. **ThermalComponent** - temperature, heat capacity

---

## Entity Components (Realistic, Physics-Based)

### 1. BiologicalPropertiesComponent
**Purpose**: Physical properties of biological tissues for ballistics calculations

```cpp
struct BiologicalPropertiesComponent {
    // Tissue Material Properties (for ballistics penetration)
    struct TissueMaterial {
        physics::Material material;     // Density, hardness from physics system
        float hydration = 0.7f;         // Water content (affects energy transfer)
        float elasticity = 0.3f;        // Tissue elasticity (bounce back)
    };

    // Body tissue types
    TissueMaterial flesh{
        .material = {
            .density = 1060.0f,         // kg/m³ (human muscle)
            .hardness = 0.2f,            // Soft tissue (0-10 Mohs scale)
            .friction = 0.8f
        },
        .hydration = 0.75f,
        .elasticity = 0.3f
    };

    TissueMaterial bone{
        .material = {
            .density = 1900.0f,         // kg/m³ (cortical bone)
            .hardness = 3.5f,            // Similar to calcite
            .friction = 0.5f
        },
        .hydration = 0.15f,
        .elasticity = 0.1f
    };

    TissueMaterial fat{
        .material = {
            .density = 900.0f,          // kg/m³
            .hardness = 0.1f,            // Very soft
            .friction = 0.9f
        },
        .hydration = 0.2f,
        .elasticity = 0.5f
    };

    // Body composition (percentages)
    float muscle_mass_kg = 30.0f;      // Actual muscle mass
    float bone_mass_kg = 10.0f;        // Skeleton mass
    float fat_mass_kg = 15.0f;         // Body fat
    float blood_volume_liters = 5.0f;  // Typical adult human

    // Physical attributes (REAL, not D&D)
    float reaction_time_ms = 200.0f;   // Milliseconds to react
    float nerve_conduction_speed = 120.0f; // m/s (motor neurons)
    float max_oxygen_uptake = 3.5f;    // VO2 max (L/min)

    /**
     * @brief Get material at body location (for ballistics penetration)
     */
    const TissueMaterial& get_tissue_material(AnatomyComponent::BodyPart part) const {
        switch (part) {
            case AnatomyComponent::BodyPart::Head:
                return bone; // Skull is hard
            case AnatomyComponent::BodyPart::Torso:
                return flesh; // Mostly muscle/organs
            default:
                return flesh; // Limbs are muscle/bone mix
        }
    }

    /**
     * @brief Calculate penetration depth for projectile
     * Uses physics::ProjectileComponent and Material properties
     */
    float calculate_penetration_depth(
        float projectile_mass_kg,
        float projectile_velocity_m_s,
        const TissueMaterial& tissue
    ) const {
        // Kinetic energy (Joules)
        float kinetic_energy = 0.5f * projectile_mass_kg *
                              projectile_velocity_m_s * projectile_velocity_m_s;

        // Penetration resistance (tissue density * hardness)
        float resistance = tissue.material.density * tissue.material.hardness;

        // Penetration depth (meters) - simplified but physics-based
        // Real formula would use sectional density, but this is reasonable
        float penetration_m = kinetic_energy / (resistance * 100.0f);

        // Hydration increases energy transfer (hydrostatic shock)
        penetration_m *= (1.0f + tissue.hydration * 0.5f);

        return penetration_m;
    }
};
```

---

### 2. Anatomy Component (Physics-Based)
**Purpose**: Organ locations and sizes for ballistics trajectory

```cpp
struct AnatomyComponent {
    enum class BodyPart : uint8_t {
        Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg
    };

    /// Organ with physical properties and location
    struct Organ {
        std::string name;
        math::Vec3 position;           // Relative to body center (meters)
        float radius;                  // Bounding sphere radius (meters)
        float mass_kg;                 // Actual organ mass
        float function = 1.0f;         // 0.0-1.0 (damaged state)
        bool is_critical = false;      // Instant death if destroyed?

        // Damage state
        float accumulated_energy_j = 0.0f;  // Total energy absorbed (Joules)
        float bleeding_rate = 0.0f;         // Blood loss (liters/second)
    };

    // Organ definitions with REAL positions and masses
    std::unordered_map<std::string, Organ> organs = {
        {"brain", {
            "brain",
            math::Vec3{0.0f, 1.6f, 0.0f},  // 1.6m above center (head)
            0.08f,                          // ~8cm radius
            1.4f,                           // 1.4kg mass
            1.0f, true, 0.0f, 0.0f
        }},
        {"heart", {
            "heart",
            math::Vec3{-0.05f, 1.2f, 0.1f}, // Left of center, chest
            0.06f,                          // ~6cm radius
            0.3f,                           // 300g mass
            1.0f, true, 0.0f, 0.0f
        }},
        {"lungs", {
            "lungs",
            math::Vec3{0.0f, 1.2f, 0.0f},   // Center chest
            0.15f,                          // Large organ
            1.1f,                           // ~1.1kg total
            1.0f, false, 0.0f, 0.0f
        }},
        // Add more organs...
    };

    float blood_volume_liters = 5.0f;

    /**
     * @brief Check if projectile trajectory hits organ
     *
     * Uses raycast against organ bounding spheres
     * This is where ballistics meets anatomy!
     */
    std::vector<std::string> check_trajectory_hits(
        const math::Vec3& entry_point,
        const math::Vec3& direction,
        float penetration_depth
    ) const {
        std::vector<std::string> hit_organs;

        // Cast ray through body
        math::geometry::Ray trajectory{entry_point, direction};

        for (const auto& [name, organ] : organs) {
            // Check if ray intersects organ sphere
            math::geometry::Sphere organ_sphere{organ.position, organ.radius};

            float t;
            if (math::geometry::intersect_ray_sphere(trajectory, organ_sphere, t)) {
                // Check if penetration reaches this organ
                float distance_to_organ = math::length(organ.position - entry_point);
                if (distance_to_organ <= penetration_depth) {
                    hit_organs.push_back(name);
                }
            }
        }

        return hit_organs;
    }

    /**
     * @brief Apply energy damage to organ
     *
     * @param organ_name Which organ was hit
     * @param energy_joules Kinetic energy transferred (from ballistics)
     */
    void apply_energy_damage(const std::string& organ_name, float energy_joules) {
        auto it = organs.find(organ_name);
        if (it == organs.end()) return;

        Organ& organ = it->second;

        // Accumulate energy
        organ.accumulated_energy_j += energy_joules;

        // Function degrades with energy (empirical formula)
        // ~50 Joules destroys soft tissue, ~500 Joules damages bone
        float damage_threshold = organ.is_critical ? 30.0f : 50.0f;
        float damage_ratio = organ.accumulated_energy_j / damage_threshold;

        organ.function = std::clamp(1.0f - damage_ratio, 0.0f, 1.0f);

        // Bleeding based on energy and vascularization
        if (organ_name == "heart" || organ_name == "lungs") {
            organ.bleeding_rate += energy_joules * 0.001f; // Highly vascular
        } else {
            organ.bleeding_rate += energy_joules * 0.0001f;
        }
    }

    /**
     * @brief Update physiology (called per frame)
     */
    void update(float delta_time) {
        // Blood loss from all bleeding organs
        float total_bleeding = 0.0f;
        for (auto& [name, organ] : organs) {
            total_bleeding += organ.bleeding_rate;

            // Bleeding decreases slowly (clotting)
            organ.bleeding_rate *= 0.99f;
        }

        blood_volume_liters -= total_bleeding * delta_time;
        blood_volume_liters = std::clamp(blood_volume_liters, 0.0f, 10.0f);
    }

    /**
     * @brief Check if alive (NO HITPOINTS!)
     */
    bool is_alive() const {
        // Dead if critical organ destroyed
        if (organs.at("brain").function <= 0.0f) return false;
        if (organs.at("heart").function <= 0.0f) return false;

        // Dead if exsanguination (>60% blood loss)
        if (blood_volume_liters < 2.0f) return false;

        return true;
    }

    /**
     * @brief Create human anatomy
     */
    static AnatomyComponent create_human();
};
```

---

### 3. Synthetic Component System
**Purpose**: Robots/drones with component-based damage

```cpp
struct SyntheticComponentSystem {
    struct Component {
        std::string name;
        math::Vec3 position;       // Relative position
        float integrity = 1.0f;    // 0=destroyed, 1=pristine
        physics::Material material; // Metal properties
        float mass_kg;
        bool is_critical = false;

        // Damage state
        float accumulated_energy_j = 0.0f;
    };

    std::unordered_map<std::string, Component> components = {
        {"power_core", {
            "power_core",
            math::Vec3{0.0f, 0.5f, 0.0f},
            1.0f,
            physics::Material{
                .density = 7850.0f,  // Steel
                .hardness = 4.5f,    // Mohs scale
                .electrical_conductivity = 1.0f
            },
            2.0f, true, 0.0f
        }},
        // More components...
    };

    /**
     * @brief Calculate armor penetration
     * Uses projectile energy vs armor hardness
     */
    bool penetrates_armor(
        float projectile_energy_j,
        const Component& component
    ) const {
        // Armor resistance (simplified but physics-based)
        float armor_resistance = component.material.density *
                                component.material.hardness * 0.1f;

        return projectile_energy_j > armor_resistance;
    }
};
```

---

## Ballistics Damage Flow (Using Existing Systems)

### When Projectile Hits Entity:

```cpp
void on_projectile_impact(
    Entity projectile_entity,
    Entity target_entity,
    const math::Vec3& hit_position,
    const math::Vec3& hit_normal
) {
    // 1. Get projectile properties (from existing RigidBodyComponent)
    auto& projectile_body = world.get_component<physics::RigidBodyComponent>(projectile_entity);
    auto& projectile_comp = world.get_component<physics::ProjectileComponent>(projectile_entity);

    float projectile_mass = projectile_body.mass;
    float projectile_velocity = math::length(projectile_body.velocity);

    // 2. Calculate kinetic energy (PHYSICS!)
    float kinetic_energy_j = 0.5f * projectile_mass * projectile_velocity * projectile_velocity;

    // 3. Get target material properties
    if (world.has_component<BiologicalPropertiesComponent>(target_entity)) {
        auto& bio = world.get_component<BiologicalPropertiesComponent>(target_entity);
        auto& anatomy = world.get_component<AnatomyComponent>(target_entity);

        // Determine body part hit
        AnatomyComponent::BodyPart body_part = determine_body_part(hit_position, target_entity);

        // Get tissue material
        const auto& tissue = bio.get_tissue_material(body_part);

        // 4. Calculate penetration depth (BALLISTICS!)
        float penetration_m = bio.calculate_penetration_depth(
            projectile_mass,
            projectile_velocity,
            tissue
        );

        // 5. Check which organs are hit (ANATOMY!)
        math::Vec3 trajectory_direction = math::normalize(projectile_body.velocity);
        auto hit_organs = anatomy.check_trajectory_hits(
            hit_position,
            trajectory_direction,
            penetration_m
        );

        // 6. Apply energy damage to hit organs
        for (const std::string& organ_name : hit_organs) {
            anatomy.apply_energy_damage(organ_name, kinetic_energy_j);
        }
    }
    else if (world.has_component<SyntheticComponentSystem>(target_entity)) {
        // Handle robot damage similarly
        // Check armor penetration, damage components
    }
}
```

---

## Integration with Existing Systems

### Use BallisticsSystem
- Already calculates projectile trajectories
- Handles wind, drag, gravity
- We just need to add impact handling

### Use Material System
- Entities have Material properties
- Ballistics uses these for penetration
- Thermodynamics uses these for heat transfer

### Use ThermalComponent
- Laser damage → increase temperature
- Temperature affects organ function
- Fire, cold, thermal weapons

---

## Why This Is Better

### ❌ Old Way (Gamey):
- "Roll d20 + STR vs AC"
- "Deal 15 damage"
- "HP: 50/100"

### ✅ New Way (Physics):
- Calculate projectile kinetic energy: 1500 Joules
- Calculate tissue penetration: 0.15m based on density
- Raycast through anatomy: hits lung
- Transfer energy to lung: function drops to 60%
- Bleeding: 0.05 L/s blood loss
- Result: Entity can still fight but reduced stamina

---

## Summary

**We're using:**
- ✅ Existing physics::Material for tissue/armor properties
- ✅ Existing physics::RigidBodyComponent for mass/velocity
- ✅ Existing physics::ProjectileComponent for ballistics
- ✅ Existing physics::BallisticsSystem for trajectories
- ✅ Existing physics::ThermalComponent for thermal damage

**We're adding:**
- BiologicalPropertiesComponent - tissue materials for penetration calculations
- AnatomyComponent - organ positions and energy-based damage
- SyntheticComponentSystem - robot components with armor
- Impact handler that uses ballistics to calculate damage

**No D&D mechanics. Pure physics.**