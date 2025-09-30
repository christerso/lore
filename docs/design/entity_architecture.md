# Entity Architecture Design
## Core Entity Foundation - Simple but Realistic

**Design Principle**: NOT over-engineered. Realistic without unnecessary complexity. NO hitpoints.

---

## Overview

Entities are composed of components that define their physical, mental, and systemic properties. Health is determined by **actual organ/component function**, not abstract hitpoints.

---

## Core Components

### 1. PhysicalTraitsComponent
**Purpose**: Physical properties that affect gameplay mechanics

```cpp
struct PhysicalTraitsComponent {
    // Size and Mass
    float weight_kg = 70.0f;        // Affects carrying capacity, fall damage
    float height_m = 1.75f;          // Affects vision height, stealth, equipment fit

    // Body Size Category (affects equipment compatibility)
    enum class SizeCategory {
        Tiny,       // < 0.5m (rats, small drones)
        Small,      // 0.5-1.2m (children, small creatures)
        Medium,     // 1.2-2.4m (humans, most humanoids)
        Large,      // 2.4-4.8m (ogres, vehicles)
        Huge        // > 4.8m (giants, mechs)
    } size = SizeCategory::Medium;

    // Movement (derived from size, weight, leg function)
    float base_move_speed = 1.0f;   // Multiplier, modified by leg damage
};
```

**Simple, Not Over-Engineered**:
- ❌ No bone density simulation
- ❌ No individual muscle fiber tracking
- ✅ Just what affects gameplay: can they wear this armor? How fast do they move?

---

### 2. AttributesComponent
**Purpose**: Core stats that affect ALL game mechanics

```cpp
struct AttributesComponent {
    // Core Attributes (1-20 scale, 10 is average human)
    int8_t strength = 10;       // Melee damage, carrying capacity
    int8_t dexterity = 10;      // Accuracy, dodge, stealth
    int8_t constitution = 10;   // Resilience to damage, stamina
    int8_t intelligence = 10;   // Tech use, problem solving
    int8_t wisdom = 10;         // Perception, awareness, judgment
    int8_t charisma = 10;       // Social interactions, leadership

    // Derived Values (calculated, not stored as HP)
    float get_carrying_capacity() const {
        return strength * 5.0f; // kg
    }

    float get_stamina_pool() const {
        return constitution * 10.0f; // stamina points
    }

    int get_attribute_modifier(int8_t attribute) const {
        return (attribute - 10) / 2; // D&D style modifier
    }
};
```

**Simple, Not Over-Engineered**:
- ❌ No 50 different stats
- ❌ No complex stat interdependencies
- ✅ Just 6 core stats that affect everything

---

### 3. SkillsComponent
**Purpose**: Learned proficiencies

```cpp
struct SkillsComponent {
    // Skill levels (0-20, 0=untrained, 10=professional, 20=master)
    std::unordered_map<std::string, int8_t> skills;

    // Common skills
    static constexpr const char* MELEE_COMBAT = "melee_combat";
    static constexpr const char* RANGED_COMBAT = "ranged_combat";
    static constexpr const char* STEALTH = "stealth";
    static constexpr const char* PERCEPTION = "perception";
    static constexpr const char* ENGINEERING = "engineering";
    static constexpr const char* MEDICINE = "medicine";

    // Query skill level
    int8_t get_skill(const char* skill_name) const {
        auto it = skills.find(skill_name);
        return (it != skills.end()) ? it->second : 0;
    }

    // Skill check: roll + skill + attribute modifier
    bool skill_check(const char* skill_name, int difficulty,
                     const AttributesComponent& attrs) const;
};
```

**Simple, Not Over-Engineered**:
- ❌ No complex skill trees
- ❌ No hundreds of micro-skills
- ✅ Flexible string-based system, add skills as needed

---

### 4. AnatomyComponent (Biological Entities)
**Purpose**: Organ-based health - NO HITPOINTS

```cpp
struct AnatomyComponent {
    // Body Regions
    enum class BodyPart {
        Head,           // Brain, eyes, ears
        Torso,          // Heart, lungs, stomach
        LeftArm,
        RightArm,
        LeftLeg,
        RightLeg
    };

    // Organs (function 0.0-1.0, 0=destroyed, 1=healthy)
    struct Organ {
        float function = 1.0f;      // How well it works
        float bleeding = 0.0f;      // Blood loss rate (0-1)
        bool is_critical = false;   // Immediate death if destroyed?
    };

    std::unordered_map<std::string, Organ> organs = {
        {"brain", {1.0f, 0.0f, true}},      // Critical: instant death if destroyed
        {"heart", {1.0f, 0.0f, true}},      // Critical: death in seconds
        {"lungs", {1.0f, 0.0f, false}},     // Reduced stamina, death in minutes
        {"left_arm", {1.0f, 0.0f, false}},  // Can't use left arm
        {"right_arm", {1.0f, 0.0f, false}}, // Can't use right arm
        {"left_leg", {1.0f, 0.0f, false}},  // Reduced movement
        {"right_leg", {1.0f, 0.0f, false}}  // Reduced movement
    };

    // Blood loss (separate from organs)
    float blood_volume = 1.0f;      // 1.0 = full, 0.0 = exsanguinated

    // Death determination (NO HITPOINTS!)
    bool is_alive() const {
        // Dead if critical organ destroyed
        if (organs.at("brain").function <= 0.0f) return false;
        if (organs.at("heart").function <= 0.0f) return false;

        // Dead if blood loss too severe
        if (blood_volume <= 0.2f) return false;

        return true;
    }

    // Capability checks
    bool can_walk() const {
        float leg_function = (organs.at("left_leg").function +
                             organs.at("right_leg").function) / 2.0f;
        return leg_function > 0.3f; // Need at least one functional leg
    }

    bool can_use_two_handed() const {
        return organs.at("left_arm").function > 0.5f &&
               organs.at("right_arm").function > 0.5f;
    }

    // Apply damage to body part
    void take_damage(BodyPart part, float damage);
};
```

**Simple, Not Over-Engineered**:
- ❌ No modeling every cell, every blood vessel
- ❌ No complex organ interdependencies (except critical ones)
- ✅ Organs have function levels
- ✅ Damage reduces function
- ✅ Zero function = organ failure
- ✅ Death from critical organ failure or blood loss

---

### 5. ComponentSystemComponent (Synthetic Entities)
**Purpose**: Component-based health for robots/drones

```cpp
struct ComponentSystemComponent {
    // Synthetic "organs" (power, computation, mobility, etc.)
    enum class ComponentType {
        PowerCore,      // Powers everything
        Processor,      // Intelligence/control
        Sensors,        // Vision, hearing
        LeftActuator,   // Movement
        RightActuator,
        Weapon,
        Armor
    };

    struct Component {
        float integrity = 1.0f;     // 0=destroyed, 1=pristine
        float power_draw = 0.0f;    // Watts needed
        bool is_critical = false;   // System shutdown if destroyed?
    };

    std::unordered_map<std::string, Component> components = {
        {"power_core", {1.0f, 0.0f, true}},     // Critical
        {"processor", {1.0f, 50.0f, true}},     // Critical
        {"sensors", {1.0f, 20.0f, false}},      // Blind if destroyed
        {"left_actuator", {1.0f, 100.0f, false}},
        {"right_actuator", {1.0f, 100.0f, false}}
    };

    float available_power = 1000.0f;    // Total power budget

    // Functional checks
    bool is_operational() const {
        return components.at("power_core").integrity > 0.0f &&
               components.at("processor").integrity > 0.0f;
    }

    bool can_move() const {
        return (components.at("left_actuator").integrity > 0.3f ||
                components.at("right_actuator").integrity > 0.3f);
    }

    // Power management
    float get_total_power_draw() const;
    bool has_sufficient_power() const;
};
```

**Simple, Not Over-Engineered**:
- ❌ No electrical circuit simulation
- ❌ No thermal management subsystems
- ✅ Components have integrity
- ✅ Damaged components = reduced capability
- ✅ Destroyed critical component = shutdown

---

## Damage System (NOT over-engineered!)

### Hit Location
```cpp
enum class HitLocation {
    Head,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg
};

// Simple hit location determination
HitLocation determine_hit_location(const math::Vec3& attack_direction,
                                   const math::Vec3& target_orientation) {
    // Simple raycast: which body part did we hit?
    // NOT: complex polygon collision with every bone
}
```

### Damage Application
```cpp
void apply_damage(Entity entity, HitLocation location, float damage) {
    if (has_component<AnatomyComponent>(entity)) {
        // Biological damage
        auto& anatomy = get_component<AnatomyComponent>(entity);
        anatomy.take_damage(location, damage);

        // Organs can fail gradually
        // Example: lung at 50% function = 50% stamina pool
    }
    else if (has_component<ComponentSystemComponent>(entity)) {
        // Synthetic damage
        auto& components = get_component<ComponentSystemComponent>(entity);
        components.take_damage(location, damage);

        // Components can malfunction or fail
    }
}
```

**Simple, Not Over-Engineered**:
- ❌ No complex wound simulation
- ❌ No infection/disease spread (unless you want that later)
- ✅ Damage → Reduced organ function → Reduced capabilities
- ✅ Critical damage → Death

---

## Usage Examples

### Create a Human
```cpp
Entity human = world.create_entity();

world.add_component<PhysicalTraitsComponent>(human, {
    .weight_kg = 75.0f,
    .height_m = 1.80f,
    .size = PhysicalTraitsComponent::SizeCategory::Medium
});

world.add_component<AttributesComponent>(human, {
    .strength = 12,
    .dexterity = 10,
    .constitution = 11,
    .intelligence = 10,
    .wisdom = 9,
    .charisma = 8
});

world.add_component<SkillsComponent>(human, {
    .skills = {
        {"melee_combat", 5},
        {"ranged_combat", 3},
        {"stealth", 2}
    }
});

world.add_component<AnatomyComponent>(human); // Defaults to healthy human
```

### Apply Damage
```cpp
// Shoot entity in the torso
apply_damage(human, HitLocation::Torso, 30.0f);

// Check if they can still fight
auto& anatomy = world.get_component<AnatomyComponent>(human);
if (anatomy.organs.at("lungs").function < 0.5f) {
    // Reduced stamina, can't sprint
}

if (!anatomy.is_alive()) {
    // Entity died from injuries
}
```

### Create a Robot
```cpp
Entity robot = world.create_entity();

world.add_component<PhysicalTraitsComponent>(robot, {
    .weight_kg = 200.0f,
    .height_m = 2.0f,
    .size = PhysicalTraitsComponent::SizeCategory::Large
});

world.add_component<AttributesComponent>(robot, {
    .strength = 16,
    .dexterity = 8,
    .constitution = 14, // Durability
    .intelligence = 12,
    .wisdom = 10,
    .charisma = 1
});

world.add_component<ComponentSystemComponent>(robot); // Defaults to functional robot
```

---

## What We're NOT Doing (Avoiding Over-Engineering)

❌ **NOT modeling**:
- Individual bones and fractures
- Blood chemistry and toxins
- Nerve impulses and brain signals
- Detailed organ interdependencies (liver failure → kidney failure → etc.)
- Muscle fatigue at fiber level
- Specific wound types (laceration vs puncture vs burn)
- Circuit diagrams for robots
- Thermal dissipation for synthetics

✅ **JUST modeling**:
- Can this entity function? (organs/components working?)
- What can they do? (walk, fight, see, think?)
- Are they alive/operational? (critical systems functional?)
- How does damage affect them? (reduced capabilities)

---

## Integration with Existing Systems

### Vision System
```cpp
// Vision affected by organ damage
if (anatomy.organs.at("eyes").function < 0.5f) {
    vision.perception_multiplier *= 0.5f; // Blurry vision
}
```

### Equipment System (Future)
```cpp
// Can they wear this armor?
auto& traits = get_component<PhysicalTraitsComponent>(entity);
if (armor.required_size != traits.size) {
    // Armor doesn't fit!
}

if (anatomy.organs.at("left_arm").function < 0.5f) {
    // Can't equip two-handed weapons
}
```

### Movement System (Future)
```cpp
// Movement speed based on leg function
float move_speed = traits.base_move_speed;
if (has_component<AnatomyComponent>(entity)) {
    auto& anatomy = get_component<AnatomyComponent>(entity);
    float leg_function = (anatomy.organs.at("left_leg").function +
                         anatomy.organs.at("right_leg").function) / 2.0f;
    move_speed *= leg_function;
}
```

---

## Summary

**This design is**:
- ✅ Simple (6 attributes, basic organs/components)
- ✅ Realistic (organ function determines health)
- ✅ NOT over-engineered (no unnecessary complexity)
- ✅ Extensible (add organs/components as needed)
- ✅ NO HITPOINTS (function-based health)

**Ready to implement!**