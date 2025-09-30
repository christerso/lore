#include "lore/ecs/systems/thermal_system.hpp"
#include "lore/ecs/components/chemical_composition_component.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace lore::ecs {

namespace {
    // Physical constants
    constexpr float STEFAN_BOLTZMANN = 5.67e-8f;  // W/(m²·K⁴)
    constexpr float ABSOLUTE_ZERO = 0.0f;
    constexpr float MIN_TEMPERATURE = 1.0f;       // Minimum allowed temp (1K)
    constexpr float MAX_TEMPERATURE = 10000.0f;   // Maximum allowed temp (10000K)
}

ThermalSystem::ThermalSystem() {
    config_ = Config{};
}

void ThermalSystem::initialize(const Config& config) {
    config_ = config;
    accumulated_time_ = 0.0f;
    stats_ = Stats{};
}

void ThermalSystem::update(World& world, float delta_time_s) {
    if (delta_time_s <= 0.0f) return;

    // Accumulate time for fixed timestep
    accumulated_time_ += delta_time_s;
    const float fixed_dt = 1.0f / config_.update_rate_hz;

    if (accumulated_time_ < fixed_dt) {
        return;  // Not time to update yet
    }

    // Reset stats
    stats_ = Stats{};

    // Build spatial grid for efficient queries
    if (config_.use_spatial_partitioning) {
        build_spatial_grid(world);
    }

    // Process all entities with thermal properties
    world.view<ThermalPropertiesComponent>([&](Entity entity, ThermalPropertiesComponent& thermal) {
        stats_.entities_processed++;

        // Get entity position (assume entities have transform - simplified)
        math::Vec3 position{0.0f, 0.0f, 0.0f};  // TODO: Get from transform component

        // Process heat transfer with neighbors
        if (config_.enable_heat_transfer) {
            process_heat_transfer(world, entity, position, thermal, fixed_dt);
        }

        // Convective cooling to ambient air
        if (thermal.current_temperature_k > config_.ambient_temperature_k) {
            float heat_lost_j = calculate_convection(thermal, fixed_dt);
            thermal.add_thermal_energy(-heat_lost_j, thermal.mass_kg);
        }

        // Check for phase transitions
        if (config_.enable_phase_transitions) {
            if (check_phase_transition(world, entity, thermal)) {
                stats_.phase_transitions++;
            }
        }

        // Check for auto-ignition
        if (config_.enable_ignition_checks) {
            if (check_ignition(world, entity, thermal)) {
                stats_.ignitions_triggered++;
            }
        }

        // Apply thermal damage to anatomy
        if (config_.enable_thermal_damage) {
            apply_thermal_damage(world, entity, thermal, fixed_dt);
        }

        // Accumulate average temperature
        stats_.avg_temperature_k += thermal.current_temperature_k;

        // Clamp temperature to valid range
        thermal.current_temperature_k = std::clamp(
            thermal.current_temperature_k,
            MIN_TEMPERATURE,
            MAX_TEMPERATURE
        );
    });

    // Calculate average temperature
    if (stats_.entities_processed > 0) {
        stats_.avg_temperature_k /= static_cast<float>(stats_.entities_processed);
    }

    // Consume accumulated time
    accumulated_time_ -= fixed_dt;
}

float ThermalSystem::apply_kinetic_heating(
    World& world,
    Entity target_entity,
    float projectile_mass_kg,
    float projectile_velocity_m_s,
    float conversion_efficiency
) {
    auto* thermal = world.get_component<ThermalPropertiesComponent>(target_entity);
    if (!thermal) return 0.0f;

    return thermal->apply_kinetic_heating(
        projectile_mass_kg,
        projectile_velocity_m_s,
        thermal->mass_kg,
        conversion_efficiency
    );
}

float ThermalSystem::apply_heat(
    World& world,
    Entity entity,
    float heat_energy_j
) {
    auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    if (!thermal) return 0.0f;

    return thermal->add_thermal_energy(heat_energy_j, thermal->mass_kg);
}

float ThermalSystem::get_temperature(const World& world, Entity entity) const {
    const auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    return thermal ? thermal->current_temperature_k : 0.0f;
}

bool ThermalSystem::can_ignite(const World& world, Entity entity) const {
    const auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    if (!thermal || thermal->ignition_temperature_k <= 0.0f) {
        return false;
    }

    return thermal->current_temperature_k >= thermal->ignition_temperature_k;
}

void ThermalSystem::set_temperature(World& world, Entity entity, float temperature_k) {
    auto* thermal = world.get_component<ThermalPropertiesComponent>(entity);
    if (!thermal) return;

    temperature_k = std::clamp(temperature_k, MIN_TEMPERATURE, MAX_TEMPERATURE);
    thermal->current_temperature_k = temperature_k;

    // Update phase state based on new temperature
    check_phase_transition(world, entity, *thermal);
}

void ThermalSystem::build_spatial_grid(World& world) {
    spatial_grid_.clear();

    world.view<ThermalPropertiesComponent>([&](Entity entity, const ThermalPropertiesComponent&) {
        // TODO: Get actual position from transform component
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        int64_t key = get_spatial_key(position);
        spatial_grid_[key].entities.push_back(entity);
    });
}

int64_t ThermalSystem::get_spatial_key(const math::Vec3& position) const {
    int32_t x = static_cast<int32_t>(std::floor(position.x / config_.spatial_grid_cell_size_m));
    int32_t y = static_cast<int32_t>(std::floor(position.y / config_.spatial_grid_cell_size_m));
    int32_t z = static_cast<int32_t>(std::floor(position.z / config_.spatial_grid_cell_size_m));

    // Morton encoding for spatial key
    int64_t key = 0;
    key |= (static_cast<int64_t>(x) & 0xFFFF) << 0;
    key |= (static_cast<int64_t>(y) & 0xFFFF) << 16;
    key |= (static_cast<int64_t>(z) & 0xFFFF) << 32;
    return key;
}

std::vector<Entity> ThermalSystem::get_nearby_entities(
    const math::Vec3& position,
    float radius_m
) const {
    std::vector<Entity> nearby;

    // Calculate cell range
    int32_t cell_radius = static_cast<int32_t>(std::ceil(radius_m / config_.spatial_grid_cell_size_m));

    // Get base cell
    int32_t base_x = static_cast<int32_t>(std::floor(position.x / config_.spatial_grid_cell_size_m));
    int32_t base_y = static_cast<int32_t>(std::floor(position.y / config_.spatial_grid_cell_size_m));
    int32_t base_z = static_cast<int32_t>(std::floor(position.z / config_.spatial_grid_cell_size_m));

    // Query neighboring cells
    for (int32_t dx = -cell_radius; dx <= cell_radius; ++dx) {
        for (int32_t dy = -cell_radius; dy <= cell_radius; ++dy) {
            for (int32_t dz = -cell_radius; dz <= cell_radius; ++dz) {
                math::Vec3 cell_pos{
                    (base_x + dx) * config_.spatial_grid_cell_size_m,
                    (base_y + dy) * config_.spatial_grid_cell_size_m,
                    (base_z + dz) * config_.spatial_grid_cell_size_m
                };

                int64_t key = get_spatial_key(cell_pos);
                auto it = spatial_grid_.find(key);
                if (it != spatial_grid_.end()) {
                    nearby.insert(nearby.end(), it->second.entities.begin(), it->second.entities.end());
                }
            }
        }
    }

    return nearby;
}

void ThermalSystem::process_heat_transfer(
    World& world,
    Entity entity,
    const math::Vec3& position,
    ThermalPropertiesComponent& thermal,
    float delta_time_s
) {
    // Get nearby entities
    std::vector<Entity> nearby = config_.use_spatial_partitioning
        ? get_nearby_entities(position, config_.radiation_range_m)
        : std::vector<Entity>{};  // TODO: Fallback to all entities

    // Limit to max neighbors for performance
    if (nearby.size() > config_.max_neighbors_per_entity) {
        nearby.resize(config_.max_neighbors_per_entity);
    }

    for (Entity neighbor : nearby) {
        if (neighbor == entity) continue;

        auto* neighbor_thermal = world.get_component<ThermalPropertiesComponent>(neighbor);
        if (!neighbor_thermal) continue;

        // Skip if temperatures too similar
        float temp_diff = std::abs(thermal.current_temperature_k - neighbor_thermal->current_temperature_k);
        if (temp_diff < config_.min_temp_diff_for_transfer) {
            continue;
        }

        // TODO: Get actual neighbor position
        math::Vec3 neighbor_pos{0.0f, 0.0f, 0.0f};
        float distance = (position - neighbor_pos).length();

        // Conduction (for close entities)
        if (distance < config_.conduction_range_m) {
            float contact_area = estimate_contact_area(world, entity, neighbor);
            float heat_j = calculate_conduction(
                thermal,
                *neighbor_thermal,
                distance,
                contact_area,
                delta_time_s
            );

            if (std::abs(heat_j) > 0.001f) {
                thermal.add_thermal_energy(-heat_j, thermal.mass_kg);
                neighbor_thermal->add_thermal_energy(heat_j, neighbor_thermal->mass_kg);
                stats_.heat_transfers_performed++;
                stats_.total_heat_transferred_j += std::abs(heat_j);
            }
        }

        // Radiation (for distant entities, especially hot ones)
        if (distance < config_.radiation_range_m) {
            float heat_j = calculate_radiation(
                thermal,
                *neighbor_thermal,
                distance,
                delta_time_s
            );

            if (std::abs(heat_j) > 0.001f) {
                thermal.add_thermal_energy(-heat_j, thermal.mass_kg);
                neighbor_thermal->add_thermal_energy(heat_j, neighbor_thermal->mass_kg);
                stats_.heat_transfers_performed++;
                stats_.total_heat_transferred_j += std::abs(heat_j);
            }
        }
    }
}

float ThermalSystem::calculate_conduction(
    const ThermalPropertiesComponent& thermal_a,
    const ThermalPropertiesComponent& thermal_b,
    float distance_m,
    float contact_area_m2,
    float delta_time_s
) const {
    if (distance_m < 0.001f) distance_m = 0.001f;  // Prevent division by zero

    // Combined thermal conductivity (harmonic mean)
    float k_combined = (2.0f * thermal_a.thermal_conductivity_w_m_k * thermal_b.thermal_conductivity_w_m_k) /
                       (thermal_a.thermal_conductivity_w_m_k + thermal_b.thermal_conductivity_w_m_k + 1e-6f);

    // Fourier's law: Q = k * A * ΔT * Δt / d
    float temp_diff = thermal_a.current_temperature_k - thermal_b.current_temperature_k;
    float heat_j = k_combined * contact_area_m2 * temp_diff * delta_time_s / distance_m;

    return heat_j * config_.heat_transfer_multiplier;
}

float ThermalSystem::calculate_radiation(
    const ThermalPropertiesComponent& thermal_a,
    const ThermalPropertiesComponent& thermal_b,
    float distance_m,
    float delta_time_s
) const {
    // View factor (simplified inverse square)
    float view_factor = 1.0f / (distance_m * distance_m + 1.0f);

    // Stefan-Boltzmann law: Q = ε * σ * A * (T₁⁴ - T₂⁴) * Δt
    float t1_4 = thermal_a.current_temperature_k * thermal_a.current_temperature_k *
                 thermal_a.current_temperature_k * thermal_a.current_temperature_k;
    float t2_4 = thermal_b.current_temperature_k * thermal_b.current_temperature_k *
                 thermal_b.current_temperature_k * thermal_b.current_temperature_k;

    float avg_emissivity = (thermal_a.emissivity + thermal_b.emissivity) * 0.5f;
    float heat_j = avg_emissivity * STEFAN_BOLTZMANN * thermal_a.surface_area_m2 *
                   (t1_4 - t2_4) * view_factor * delta_time_s;

    return heat_j * config_.heat_transfer_multiplier;
}

float ThermalSystem::calculate_convection(
    const ThermalPropertiesComponent& thermal,
    float delta_time_s
) const {
    // Newton's law of cooling: Q = h * A * ΔT * Δt
    float temp_diff = thermal.current_temperature_k - config_.ambient_temperature_k;
    float heat_j = config_.convection_coefficient * thermal.surface_area_m2 * temp_diff * delta_time_s;

    return heat_j * config_.heat_transfer_multiplier;
}

bool ThermalSystem::check_phase_transition(
    World& world,
    Entity entity,
    ThermalPropertiesComponent& thermal
) {
    PhaseState old_phase = thermal.current_phase;
    PhaseState new_phase = old_phase;

    // Add hysteresis to prevent flickering
    float temp = thermal.current_temperature_k;
    float hysteresis = config_.phase_transition_hysteresis_k;

    // Solid ↔ Liquid
    if (thermal.melting_point_k > 0.0f) {
        if (old_phase == PhaseState::Solid && temp > thermal.melting_point_k + hysteresis) {
            new_phase = PhaseState::Liquid;

            // Consume latent heat of fusion
            if (config_.track_latent_heat) {
                float latent_heat = thermal.latent_heat_fusion_j_kg * thermal.mass_kg;
                thermal.add_thermal_energy(-latent_heat, thermal.mass_kg);
            }
        } else if (old_phase == PhaseState::Liquid && temp < thermal.melting_point_k - hysteresis) {
            new_phase = PhaseState::Solid;

            // Release latent heat of fusion
            if (config_.track_latent_heat) {
                float latent_heat = thermal.latent_heat_fusion_j_kg * thermal.mass_kg;
                thermal.add_thermal_energy(latent_heat, thermal.mass_kg);
            }
        }
    }

    // Liquid ↔ Gas
    if (thermal.boiling_point_k > 0.0f) {
        if (old_phase == PhaseState::Liquid && temp > thermal.boiling_point_k + hysteresis) {
            new_phase = PhaseState::Gas;

            // Consume latent heat of vaporization
            if (config_.track_latent_heat) {
                float latent_heat = thermal.latent_heat_vaporization_j_kg * thermal.mass_kg;
                thermal.add_thermal_energy(-latent_heat, thermal.mass_kg);
            }
        } else if (old_phase == PhaseState::Gas && temp < thermal.boiling_point_k - hysteresis) {
            new_phase = PhaseState::Liquid;

            // Release latent heat of condensation
            if (config_.track_latent_heat) {
                float latent_heat = thermal.latent_heat_vaporization_j_kg * thermal.mass_kg;
                thermal.add_thermal_energy(latent_heat, thermal.mass_kg);
            }
        }
    }

    // Solid → Gas (sublimation)
    if (config_.allow_sublimation && thermal.melting_point_k > 0.0f && thermal.boiling_point_k > 0.0f) {
        float sublimation_temp = (thermal.melting_point_k + thermal.boiling_point_k) * 0.5f;
        if (old_phase == PhaseState::Solid && temp > sublimation_temp + hysteresis) {
            new_phase = PhaseState::Gas;
        }
    }

    // Update phase state
    if (new_phase != old_phase) {
        thermal.current_phase = new_phase;

        if (config_.log_phase_transitions) {
            std::cout << "[ThermalSystem] Entity " << static_cast<uint32_t>(entity)
                      << " phase transition: " << static_cast<int>(old_phase)
                      << " → " << static_cast<int>(new_phase)
                      << " at " << temp << "K\n";
        }

        return true;
    }

    return false;
}

bool ThermalSystem::check_ignition(
    World& world,
    Entity entity,
    ThermalPropertiesComponent& thermal
) {
    // Already has combustion component?
    if (world.has_component<CombustionComponent>(entity)) {
        return false;
    }

    // Check if temperature exceeds ignition point
    if (thermal.ignition_temperature_k <= 0.0f) {
        return false;  // Non-combustible
    }

    if (thermal.current_temperature_k < thermal.ignition_temperature_k) {
        return false;  // Not hot enough
    }

    // Get chemical composition to determine fuel mass
    const auto* chemical = world.get_component<ChemicalCompositionComponent>(entity);
    if (!chemical || !chemical->is_combustible) {
        return false;  // Not combustible
    }

    // Ignite! Add combustion component
    CombustionComponent combustion = CombustionComponent::ignite(
        thermal.current_temperature_k,
        thermal.mass_kg  // All mass is fuel
    );

    // Copy chemical properties to combustion
    combustion.oxygen_consumption_mol_s = chemical->calculate_oxygen_consumption(
        combustion.fuel_consumption_rate_kg_s
    );
    combustion.config.smoke_particle_spawn_rate = chemical->config.smoke_density_factor * 10.0f;
    combustion.config.soot_production_rate = chemical->config.soot_production_rate;

    world.add_component(entity, combustion);

    if (config_.log_ignitions) {
        std::cout << "[ThermalSystem] Entity " << static_cast<uint32_t>(entity)
                  << " ignited at " << thermal.current_temperature_k << "K"
                  << " (ignition point: " << thermal.ignition_temperature_k << "K)\n";
    }

    return true;
}

void ThermalSystem::apply_thermal_damage(
    World& world,
    Entity entity,
    const ThermalPropertiesComponent& thermal,
    float delta_time_s
) {
    // Only apply damage if temperature is dangerous
    if (thermal.current_temperature_k < config_.burn_threshold_temp_k) {
        return;
    }

    auto* anatomy = world.get_component<AnatomyComponent>(entity);
    if (!anatomy) {
        return;  // No anatomy to damage
    }

    // Calculate thermal damage rate
    float temp_excess = thermal.current_temperature_k - config_.burn_threshold_temp_k;
    float damage_multiplier = 1.0f;

    // Instant severe burns at very high temps
    if (thermal.current_temperature_k >= config_.instant_burn_temp_k) {
        damage_multiplier = 10.0f;
    }

    // Energy needed for damage
    float heat_energy = temp_excess * thermal.specific_heat_capacity_j_kg_k * thermal.mass_kg * delta_time_s;
    float damage_points = (heat_energy / config_.damage_rate_j_per_hp) * damage_multiplier;

    if (damage_points < 0.01f) {
        return;  // Too little damage to bother
    }

    // Apply damage to random organ (simplified - could target based on location)
    if (!anatomy->organs.empty()) {
        // Find first intact organ
        for (auto& organ : anatomy->organs) {
            if (organ.integrity > 0.0f) {
                organ.integrity -= damage_points;
                organ.integrity = std::max(0.0f, organ.integrity);

                // Add thermal damage type
                organ.damage_types |= static_cast<uint32_t>(DamageType::Thermal);

                break;
            }
        }
    }
}

float ThermalSystem::estimate_contact_area(
    World& world,
    Entity entity_a,
    Entity entity_b
) const {
    // Simplified: Assume 10cm² contact area for nearby entities
    // TODO: Calculate based on actual collision geometry
    return 0.01f;  // 0.01 m² = 100 cm²
}

} // namespace lore::ecs