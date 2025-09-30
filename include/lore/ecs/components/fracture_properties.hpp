#pragma once

#include <lore/math/math.hpp>
#include <cstdint>

namespace lore::ecs {

/**
 * @brief Fracture behavior types for material-specific breakage patterns
 */
enum class FractureBehavior : uint8_t {
    Brittle,    ///< Shatters into many pieces (glass, concrete)
    Ductile,    ///< Tears/deforms before breaking (metal)
    Fibrous,    ///< Splits along grain (wood)
    Granular    ///< Crumbles into irregular chunks (brick, stone)
};

/**
 * @brief Material-specific fracture properties
 *
 * Controls how materials break when stressed beyond limits:
 * - Brittle: Glass shatters into many sharp pieces with radial cracks
 * - Ductile: Metal tears along stress lines, bends before breaking
 * - Fibrous: Wood splinters along grain, creates elongated fragments
 * - Granular: Concrete/brick crumbles into irregular chunks
 *
 * Used by VoronoiFractureSystem and SurfaceDamageComponent.
 *
 * Example:
 * @code
 * auto glass_props = FractureProperties::create_glass();
 * // Shatters into 8-40 pieces with radial patterns and sharp edges
 * @endcode
 */
struct FractureProperties {
    /// Fracture behavior type
    FractureBehavior behavior = FractureBehavior::Brittle;

    /// Minimum number of fracture pieces
    uint32_t min_fracture_pieces = 3;

    /// Maximum number of fracture pieces
    uint32_t max_fracture_pieces = 20;

    /// Radial fracture pattern strength (0.0-1.0)
    /// Higher = more radial cracks from impact point
    float radial_pattern_strength = 0.5f;

    /// Planar fracture tendency (0.0-1.0)
    /// Higher = fractures follow planes (metal tearing)
    float planar_tendency = 0.3f;

    /// Directional bias along axis (for fibrous materials)
    math::Vec3 grain_direction{0.0f, 1.0f, 0.0f}; // Y-up for wood grain

    /// Edge sharpness (0.0-1.0)
    /// 1.0 = sharp edges (glass), 0.0 = rough edges (concrete)
    float edge_sharpness = 0.5f;

    /// Fracture seed randomness (0.0-1.0)
    /// Higher = more irregular fracture patterns
    float seed_randomness = 0.5f;

    //=====================================================================
    // Material Presets (Realistic Fracture Patterns)
    //=====================================================================

    /**
     * @brief Glass fracture properties
     *
     * Characteristics:
     * - Shatters into many pieces (8-40)
     * - Strong radial crack pattern from impact
     * - Very sharp edges
     * - Clean, planar fractures
     */
    static FractureProperties create_glass() {
        return FractureProperties{
            .behavior = FractureBehavior::Brittle,
            .min_fracture_pieces = 8,
            .max_fracture_pieces = 40,
            .radial_pattern_strength = 0.9f,
            .planar_tendency = 0.7f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f},
            .edge_sharpness = 1.0f,
            .seed_randomness = 0.3f
        };
    }

    /**
     * @brief Concrete fracture properties
     *
     * Characteristics:
     * - Crumbles into irregular chunks (5-15 pieces)
     * - Moderate radial pattern
     * - Rough, crumbly edges
     * - High randomness (irregular chunks)
     */
    static FractureProperties create_concrete() {
        return FractureProperties{
            .behavior = FractureBehavior::Granular,
            .min_fracture_pieces = 5,
            .max_fracture_pieces = 15,
            .radial_pattern_strength = 0.5f,
            .planar_tendency = 0.2f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f},
            .edge_sharpness = 0.1f,
            .seed_randomness = 0.8f
        };
    }

    /**
     * @brief Metal fracture properties
     *
     * Characteristics:
     * - Tears into few pieces (1-3)
     * - Follows stress lines (high planar tendency)
     * - Smooth, deformed edges
     * - Deforms before breaking
     */
    static FractureProperties create_metal() {
        return FractureProperties{
            .behavior = FractureBehavior::Ductile,
            .min_fracture_pieces = 1,
            .max_fracture_pieces = 3,
            .radial_pattern_strength = 0.2f,
            .planar_tendency = 0.9f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f},
            .edge_sharpness = 0.3f,
            .seed_randomness = 0.3f
        };
    }

    /**
     * @brief Wood fracture properties
     *
     * Characteristics:
     * - Splinters along grain (3-8 pieces)
     * - Strong directional bias (grain direction)
     * - Elongated, fibrous fragments
     * - Moderate edge sharpness
     */
    static FractureProperties create_wood() {
        return FractureProperties{
            .behavior = FractureBehavior::Fibrous,
            .min_fracture_pieces = 3,
            .max_fracture_pieces = 8,
            .radial_pattern_strength = 0.4f,
            .planar_tendency = 0.6f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f}, // Vertical grain
            .edge_sharpness = 0.6f,
            .seed_randomness = 0.5f
        };
    }

    /**
     * @brief Brick fracture properties
     *
     * Characteristics:
     * - Crumbles into chunks (4-10 pieces)
     * - Moderate randomness
     * - Rough edges
     * - Less radial than concrete
     */
    static FractureProperties create_brick() {
        return FractureProperties{
            .behavior = FractureBehavior::Granular,
            .min_fracture_pieces = 4,
            .max_fracture_pieces = 10,
            .radial_pattern_strength = 0.4f,
            .planar_tendency = 0.3f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f},
            .edge_sharpness = 0.2f,
            .seed_randomness = 0.7f
        };
    }

    /**
     * @brief Stone fracture properties (granite, marble)
     *
     * Characteristics:
     * - Breaks into large chunks (3-8 pieces)
     * - Low radial pattern (stone is tough)
     * - Very rough edges
     * - Moderate randomness
     */
    static FractureProperties create_stone() {
        return FractureProperties{
            .behavior = FractureBehavior::Granular,
            .min_fracture_pieces = 3,
            .max_fracture_pieces = 8,
            .radial_pattern_strength = 0.3f,
            .planar_tendency = 0.4f,
            .grain_direction = math::Vec3{0.0f, 1.0f, 0.0f},
            .edge_sharpness = 0.1f,
            .seed_randomness = 0.6f
        };
    }

    /**
     * @brief Get number of fracture pieces based on impact energy
     *
     * More energy = more pieces (up to max)
     *
     * @param impact_energy_ratio Energy ratio (0.0-1.0, where 1.0 = maximum observed energy)
     * @return Number of pieces to generate
     */
    uint32_t get_piece_count(float impact_energy_ratio) const noexcept {
        // Clamp energy ratio
        impact_energy_ratio = math::clamp(impact_energy_ratio, 0.0f, 1.0f);

        // Interpolate between min and max based on energy
        float piece_count_f = static_cast<float>(min_fracture_pieces) +
                             (static_cast<float>(max_fracture_pieces - min_fracture_pieces) *
                              impact_energy_ratio);

        return static_cast<uint32_t>(piece_count_f);
    }
};

} // namespace lore::ecs