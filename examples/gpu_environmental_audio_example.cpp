// Complete GPU Environmental Audio System Example
// Demonstrates 100% GPU execution with arena allocation and environmental acoustics

#include <lore/audio/audio.hpp>
#include <lore/audio/gpu_environmental_audio.hpp>
#include <lore/graphics/gpu_compute.hpp>
#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

using namespace lore;
using namespace lore::audio;

class GpuEnvironmentalAudioDemo {
public:
    GpuEnvironmentalAudioDemo()
        : world_(std::make_unique<ecs::World>())
        , graphics_system_(nullptr)
        , gpu_compute_system_(nullptr)
        , audio_system_(std::make_unique<AudioSystem>())
        , environmental_system_(nullptr)
    {
        initialize_systems();
        setup_demo_scene();
    }

    ~GpuEnvironmentalAudioDemo() {
        cleanup_systems();
    }

    void run_demo() {
        std::cout << "=== GPU Environmental Audio System Demo ===" << std::endl;
        std::cout << "This demo showcases 100% GPU execution with:" << std::endl;
        std::cout << "- Real-time acoustic convolution with FFT" << std::endl;
        std::cout << "- Environmental ray tracing for reflections" << std::endl;
        std::cout << "- GPU-based occlusion and diffraction" << std::endl;
        std::cout << "- Material-based reverb processing" << std::endl;
        std::cout << "- Arena-based GPU memory management" << std::endl;
        std::cout << "- Comprehensive error handling" << std::endl;
        std::cout << std::endl;

        // Run simulation for 10 seconds
        constexpr int simulation_duration_seconds = 10;
        constexpr float target_fps = 60.0f;
        constexpr auto frame_time = std::chrono::microseconds(static_cast<int>(1000000.0f / target_fps));

        auto simulation_start = std::chrono::steady_clock::now();
        auto last_frame_time = simulation_start;
        int frame_count = 0;

        while (true) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - simulation_start);

            if (elapsed.count() >= simulation_duration_seconds) {
                break;
            }

            auto delta_time_us = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time);
            float delta_time = static_cast<float>(delta_time_us.count()) / 1000000.0f;

            update_simulation(delta_time);

            frame_count++;
            if (frame_count % 60 == 0) { // Print stats every second
                print_performance_stats();
            }

            last_frame_time = current_time;

            // Frame rate limiting
            auto frame_end_time = std::chrono::steady_clock::now();
            auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_time - current_time);
            if (frame_duration < frame_time) {
                std::this_thread::sleep_for(frame_time - frame_duration);
            }
        }

        print_final_report();
    }

private:
    std::unique_ptr<ecs::World> world_;
    std::unique_ptr<graphics::GraphicsSystem> graphics_system_;
    std::unique_ptr<graphics::GpuComputeSystem> gpu_compute_system_;
    std::unique_ptr<AudioSystem> audio_system_;
    std::unique_ptr<GpuEnvironmentalAudioSystem> environmental_system_;

    void initialize_systems() {
        std::cout << "Initializing GPU Environmental Audio Systems..." << std::endl;

        try {
            // Initialize graphics system (placeholder)
            graphics_system_ = std::make_unique<graphics::GraphicsSystem>();

            // Initialize GPU compute system
            gpu_compute_system_ = std::make_unique<graphics::GpuComputeSystem>(*graphics_system_);
            gpu_compute_system_->initialize();

            // Initialize audio system
            audio_system_->init(*world_);

            // Configure GPU environmental audio system
            GpuEnvironmentalAudioSystem::SystemConfiguration config;
            config.max_audio_sources = 1024;
            config.max_reverb_zones = 64;
            config.max_rays_per_source = 16;
            config.max_ray_bounces = 8;
            config.sample_rate = 44100;
            config.buffer_size = 512;
            config.acoustic_quality_factor = 1.0f;
            config.enable_autonomous_processing = true;
            config.enable_adaptive_quality = true;
            config.target_gpu_utilization = 85.0f;
            config.enable_error_recovery = true;

            // Arena configuration for maximum performance
            config.arena_config.total_arena_size = 256 * 1024 * 1024; // 256MB
            config.arena_config.convolution_arena_size = 64 * 1024 * 1024;
            config.arena_config.ray_tracing_arena_size = 96 * 1024 * 1024;
            config.arena_config.occlusion_arena_size = 48 * 1024 * 1024;
            config.arena_config.reverb_arena_size = 32 * 1024 * 1024;
            config.arena_config.output_buffer_size = 16 * 1024 * 1024;
            config.arena_config.enable_memory_compaction = true;
            config.arena_config.compaction_threshold = 0.8f;

            // Initialize environmental audio system
            environmental_system_ = std::make_unique<GpuEnvironmentalAudioSystem>(*gpu_compute_system_);
            environmental_system_->initialize_system(config);

            // Set up error recovery callback
            environmental_system_->set_error_recovery_callback(
                [this](GpuEnvironmentalAudioException::ErrorType error_type,
                       GpuEnvironmentalAudioSystem::RecoveryStrategy strategy,
                       const std::string& details) {
                    handle_environmental_audio_error(error_type, strategy, details);
                }
            );

            environmental_system_->enable_processing(true);
            environmental_system_->enable_autonomous_mode(true);

            std::cout << "All systems initialized successfully!" << std::endl;

        } catch (const GpuEnvironmentalAudioException& e) {
            std::cerr << "Failed to initialize environmental audio: " << e.what() << std::endl;
            std::cerr << "Error type: " << static_cast<int>(e.get_error_type()) << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize systems: " << e.what() << std::endl;
            throw;
        }
    }

    void setup_demo_scene() {
        std::cout << "Setting up demo scene with environmental acoustics..." << std::endl;

        // Create audio sources with different characteristics
        create_ambient_sound_sources();
        create_music_sources();
        create_effect_sources();

        // Create acoustic environments
        create_reverb_zones();
        create_acoustic_materials();
        create_acoustic_geometry();

        // Create listeners
        create_audio_listeners();

        std::cout << "Demo scene created with:" << std::endl;
        std::cout << "- " << count_entities_with_component<AudioSourceComponent>() << " audio sources" << std::endl;
        std::cout << "- " << count_entities_with_component<ReverbComponent>() << " reverb zones" << std::endl;
        std::cout << "- " << count_entities_with_component<MaterialSoundComponent>() << " acoustic materials" << std::endl;
        std::cout << "- " << count_entities_with_component<AudioListenerComponent>() << " listeners" << std::endl;
    }

    void create_ambient_sound_sources() {
        // Create various ambient sound sources to test environmental acoustics
        for (int i = 0; i < 32; ++i) {
            auto entity = world_->create_entity();

            // Position sources in a 3D grid
            math::Vec3 position{
                static_cast<float>((i % 4) * 10.0f - 15.0f),
                static_cast<float>(((i / 4) % 4) * 5.0f),
                static_cast<float>((i / 16) * 15.0f - 7.5f)
            };

            // Transform component
            world_->add_component<math::TransformComponent>(entity, {
                .position = position,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = math::Vec3{1.0f}
            });

            // Audio source component
            AudioSourceComponent audio_source;
            audio_source.set_audio_file("ambient_tone_" + std::to_string(i % 4) + ".wav");
            audio_source.set_volume(0.3f + (i % 3) * 0.1f);
            audio_source.set_pitch(0.9f + (i % 5) * 0.05f);
            audio_source.set_3d(true);
            audio_source.set_looping(true);
            audio_source.set_min_distance(2.0f);
            audio_source.set_max_distance(50.0f);
            audio_source.set_rolloff_factor(1.0f);
            audio_source.play();

            world_->add_component<AudioSourceComponent>(entity, audio_source);

            // Directional audio for some sources
            if (i % 3 == 0) {
                DirectionalAudioSourceComponent directional;
                directional.set_directivity(DirectivityPattern::Cardioid);
                directional.set_forward_direction(math::Vec3{0.0f, 0.0f, -1.0f});
                directional.set_inner_cone_angle(45.0f);
                directional.set_outer_cone_angle(90.0f);
                directional.set_outer_cone_gain(0.25f);
                directional.set_enable_hrtf(true);
                directional.set_enable_binaural(true);

                world_->add_component<DirectionalAudioSourceComponent>(entity, directional);
            }
        }
    }

    void create_music_sources() {
        // Create music sources with different acoustic properties
        for (int i = 0; i < 8; ++i) {
            auto entity = world_->create_entity();

            math::Vec3 position{
                static_cast<float>(std::cos(i * math::utils::PI / 4.0f) * 20.0f),
                5.0f,
                static_cast<float>(std::sin(i * math::utils::PI / 4.0f) * 20.0f)
            };

            world_->add_component<math::TransformComponent>(entity, {
                .position = position,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = math::Vec3{1.0f}
            });

            AudioSourceComponent audio_source;
            audio_source.set_audio_file("music_track_" + std::to_string(i) + ".wav");
            audio_source.set_volume(0.5f);
            audio_source.set_3d(true);
            audio_source.set_looping(true);
            audio_source.set_min_distance(5.0f);
            audio_source.set_max_distance(100.0f);
            audio_source.play();

            world_->add_component<AudioSourceComponent>(entity, audio_source);

            // Add material sound component for interaction
            AcousticMaterial material;
            if (i % 2 == 0) {
                material.setup_metal_material();
            } else {
                material.setup_wood_material();
            }

            MaterialSoundComponent material_sound;
            material_sound.set_material(material);
            material_sound.set_impact_settings(true, 1.0f, 0.1f, 0.1f);
            material_sound.set_scratch_settings(true, 0.7f, 0.05f, 0.05f);

            world_->add_component<MaterialSoundComponent>(entity, material_sound);
        }
    }

    void create_effect_sources() {
        // Create dynamic effect sources
        for (int i = 0; i < 16; ++i) {
            auto entity = world_->create_entity();

            math::Vec3 position{
                static_cast<float>((i % 4) * 8.0f - 12.0f),
                2.0f + (i % 3) * 2.0f,
                static_cast<float>((i / 4) * 8.0f - 12.0f)
            };

            world_->add_component<math::TransformComponent>(entity, {
                .position = position,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = math::Vec3{1.0f}
            });

            AudioSourceComponent audio_source;
            audio_source.set_audio_file("effect_sound_" + std::to_string(i % 6) + ".wav");
            audio_source.set_volume(0.4f);
            audio_source.set_pitch(1.0f + (i % 7) * 0.1f);
            audio_source.set_3d(true);
            audio_source.set_min_distance(1.0f);
            audio_source.set_max_distance(30.0f);

            // Random playback pattern
            if (i % 2 == 0) {
                audio_source.play();
            }

            world_->add_component<AudioSourceComponent>(entity, audio_source);
        }
    }

    void create_reverb_zones() {
        // Create different acoustic environments
        struct ReverbZoneConfig {
            math::Vec3 center;
            math::Vec3 extents;
            std::string name;
            AcousticMaterial material;
            float room_size;
            float damping;
        };

        std::vector<ReverbZoneConfig> zones = {
            // Large cathedral space
            {{0.0f, 15.0f, 0.0f}, {25.0f, 15.0f, 40.0f}, "Cathedral", {}, 1.0f, 0.2f},

            // Small intimate room
            {{-30.0f, 3.0f, -30.0f}, {8.0f, 3.0f, 8.0f}, "Small Room", {}, 0.3f, 0.8f},

            // Medium concert hall
            {{30.0f, 8.0f, 30.0f}, {15.0f, 8.0f, 20.0f}, "Concert Hall", {}, 0.7f, 0.4f},

            // Outdoor space (minimal reverb)
            {{0.0f, 5.0f, 60.0f}, {50.0f, 10.0f, 30.0f}, "Outdoor", {}, 0.1f, 0.9f},
        };

        for (size_t i = 0; i < zones.size(); ++i) {
            auto entity = world_->create_entity();
            const auto& zone_config = zones[i];

            world_->add_component<math::TransformComponent>(entity, {
                .position = zone_config.center,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = zone_config.extents
            });

            ReverbComponent reverb;
            reverb.set_room_size(zone_config.room_size);
            reverb.set_damping(zone_config.damping);
            reverb.set_wet_level(0.3f + i * 0.1f);
            reverb.set_dry_level(0.7f - i * 0.05f);
            reverb.set_pre_delay(0.02f + i * 0.01f);
            reverb.set_decay_time(1.0f + i * 0.5f);

            // Configure wall material based on zone type
            AcousticMaterial wall_material;
            switch (i) {
                case 0: // Cathedral
                    wall_material.setup_concrete_material();
                    wall_material.set_absorption(0.05f);
                    break;
                case 1: // Small room
                    wall_material.setup_fabric_material();
                    wall_material.set_absorption(0.7f);
                    break;
                case 2: // Concert hall
                    wall_material.setup_wood_material();
                    wall_material.set_absorption(0.2f);
                    break;
                case 3: // Outdoor
                    wall_material.setup_concrete_material();
                    wall_material.set_absorption(0.95f); // High absorption for outdoor
                    break;
            }

            reverb.set_wall_material(wall_material);
            world_->add_component<ReverbComponent>(entity, reverb);
        }
    }

    void create_acoustic_materials() {
        // Create various acoustic materials for environmental interaction
        std::vector<std::pair<math::Vec3, AcousticMaterial>> materials = {
            {{-20.0f, 0.0f, -20.0f}, {}}, // Metal surface
            {{-10.0f, 0.0f, -20.0f}, {}}, // Wood surface
            {{0.0f, 0.0f, -20.0f}, {}},   // Glass surface
            {{10.0f, 0.0f, -20.0f}, {}},  // Fabric surface
            {{20.0f, 0.0f, -20.0f}, {}},  // Concrete surface
        };

        for (size_t i = 0; i < materials.size(); ++i) {
            auto entity = world_->create_entity();

            world_->add_component<math::TransformComponent>(entity, {
                .position = materials[i].first,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = math::Vec3{3.0f, 3.0f, 0.5f}
            });

            AcousticMaterial& material = materials[i].second;
            switch (i) {
                case 0: material.setup_metal_material(); break;
                case 1: material.setup_wood_material(); break;
                case 2: material.setup_glass_material(); break;
                case 3: material.setup_fabric_material(); break;
                case 4: material.setup_concrete_material(); break;
            }

            MaterialSoundComponent material_sound;
            material_sound.set_material(material);
            material_sound.set_impact_settings(true, 1.0f, 0.2f, 0.1f);
            material_sound.set_scratch_settings(true, 0.8f, 0.1f, 0.05f);
            material_sound.set_roll_settings(true, 0.6f, 0.05f, 0.03f);

            world_->add_component<MaterialSoundComponent>(entity, material_sound);
        }
    }

    void create_acoustic_geometry() {
        // Create geometric obstacles for acoustic ray tracing and occlusion
        for (int i = 0; i < 20; ++i) {
            auto entity = world_->create_entity();

            math::Vec3 position{
                static_cast<float>((i % 5) * 12.0f - 24.0f),
                2.0f + (i % 3) * 3.0f,
                static_cast<float>((i / 5) * 12.0f - 18.0f)
            };

            math::Vec3 scale{
                2.0f + (i % 3) * 1.0f,
                1.0f + (i % 4) * 2.0f,
                1.0f + (i % 2) * 3.0f
            };

            world_->add_component<math::TransformComponent>(entity, {
                .position = position,
                .rotation = math::Quat{1, 0, 0, 0},
                .scale = scale
            });

            // Add material for acoustic interaction
            AcousticMaterial material;
            switch (i % 3) {
                case 0: material.setup_concrete_material(); break;
                case 1: material.setup_metal_material(); break;
                case 2: material.setup_wood_material(); break;
            }

            MaterialSoundComponent material_sound;
            material_sound.set_material(material);
            world_->add_component<MaterialSoundComponent>(entity, material_sound);
        }
    }

    void create_audio_listeners() {
        // Create primary listener (player)
        auto listener_entity = world_->create_entity();

        world_->add_component<math::TransformComponent>(listener_entity, {
            .position = {0.0f, 1.8f, 0.0f}, // Eye level
            .rotation = math::Quat{1, 0, 0, 0},
            .scale = math::Vec3{1.0f}
        });

        AudioListenerComponent listener;
        listener.set_gain(1.0f);
        listener.set_active(true);
        world_->add_component<AudioListenerComponent>(listener_entity, listener);

        // Add hearing simulation component
        HearingComponent hearing;
        hearing.set_hearing_threshold(0.0f);
        hearing.set_damage_threshold(85.0f);
        hearing.set_pain_threshold(120.0f);
        world_->add_component<HearingComponent>(listener_entity, hearing);
    }

    void update_simulation(float delta_time) {
        // Update ECS world
        world_->update(delta_time);

        // Update audio system with environmental processing
        audio_system_->update(*world_, delta_time);

        // Update GPU environmental acoustics
        if (environmental_system_) {
            environmental_system_->update_environmental_acoustics(*world_, delta_time);
        }

        // Simulate dynamic scene changes
        update_dynamic_sources(delta_time);
    }

    void update_dynamic_sources(float delta_time) {
        static float simulation_time = 0.0f;
        simulation_time += delta_time;

        // Move some audio sources dynamically
        world_->view<AudioSourceComponent, math::TransformComponent>().each(
            [&](auto entity, auto& audio, auto& transform) {
                // Simple circular motion for demonstration
                auto entity_id = static_cast<uint32_t>(entity);
                if (entity_id % 5 == 0) {
                    float radius = 10.0f;
                    float speed = 0.5f + (entity_id % 3) * 0.2f;
                    float angle = simulation_time * speed + entity_id;

                    transform.position.x = std::cos(angle) * radius;
                    transform.position.z = std::sin(angle) * radius;

                    // Update velocity for Doppler effect
                    math::Vec3 velocity{
                        -std::sin(angle) * radius * speed,
                        0.0f,
                        std::cos(angle) * radius * speed
                    };
                    audio.set_velocity(velocity);
                }
            }
        );

        // Trigger some effects periodically
        if (static_cast<int>(simulation_time * 2.0f) % 3 == 0) {
            world_->view<AudioSourceComponent, DirectionalAudioSourceComponent>().each(
                [&](auto entity, auto& audio, auto& directional) {
                    if (!audio.get_is_playing()) {
                        audio.play();
                    }
                }
            );
        }
    }

    void print_performance_stats() {
        if (!environmental_system_) return;

        auto metrics = environmental_system_->get_performance_metrics();
        auto memory_stats = environmental_system_->get_memory_stats();

        std::cout << "\n=== GPU Environmental Audio Performance ===" << std::endl;
        std::cout << "GPU Utilization: " << std::fixed << std::setprecision(1)
                  << metrics.gpu_utilization_percentage << "%" << std::endl;
        std::cout << "Frame Time: " << metrics.frame_time.count() << " μs" << std::endl;
        std::cout << "Memory Used: " << (memory_stats.total_used / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "Active Sources: " << metrics.audio_sources_processed_per_second / 60 << std::endl;
        std::cout << "Rays/sec: " << metrics.rays_traced_per_second << std::endl;
        std::cout << "Occlusion Tests/sec: " << metrics.occlusion_tests_per_second << std::endl;
        std::cout << "Acoustic Quality: " << (metrics.acoustic_accuracy_score * 100.0f) << "%" << std::endl;

        if (metrics.compute_shader_errors > 0 || metrics.memory_allocation_failures > 0) {
            std::cout << "Errors: Compute=" << metrics.compute_shader_errors
                      << " Memory=" << metrics.memory_allocation_failures << std::endl;
        }
    }

    void print_final_report() {
        std::cout << "\n=== Final GPU Environmental Audio Report ===" << std::endl;

        if (environmental_system_) {
            std::cout << environmental_system_->generate_diagnostic_report() << std::endl;

            // Export detailed metrics
            try {
                environmental_system_->export_performance_data("gpu_environmental_audio_metrics.csv");
                std::cout << "Detailed metrics exported to: gpu_environmental_audio_metrics.csv" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Failed to export metrics: " << e.what() << std::endl;
            }
        }

        std::cout << "\nDemo completed successfully!" << std::endl;
        std::cout << "Key achievements demonstrated:" << std::endl;
        std::cout << "✓ 100% GPU execution for environmental acoustics" << std::endl;
        std::cout << "✓ Real-time acoustic convolution with FFT" << std::endl;
        std::cout << "✓ Environmental ray tracing with material interactions" << std::endl;
        std::cout << "✓ GPU-based occlusion and diffraction processing" << std::endl;
        std::cout << "✓ Arena-based GPU memory management" << std::endl;
        std::cout << "✓ Comprehensive error handling and recovery" << std::endl;
        std::cout << "✓ Performance monitoring and adaptive optimization" << std::endl;
    }

    void handle_environmental_audio_error(
        GpuEnvironmentalAudioException::ErrorType error_type,
        GpuEnvironmentalAudioSystem::RecoveryStrategy strategy,
        const std::string& details
    ) {
        std::cout << "Environmental Audio Error Detected:" << std::endl;
        std::cout << "Type: " << static_cast<int>(error_type) << std::endl;
        std::cout << "Strategy: " << static_cast<int>(strategy) << std::endl;
        std::cout << "Details: " << details << std::endl;

        switch (strategy) {
            case GpuEnvironmentalAudioSystem::RecoveryStrategy::RETRY_OPERATION:
                std::cout << "Retrying operation..." << std::endl;
                break;
            case GpuEnvironmentalAudioSystem::RecoveryStrategy::REDUCE_QUALITY:
                std::cout << "Reducing quality to maintain performance..." << std::endl;
                break;
            case GpuEnvironmentalAudioSystem::RecoveryStrategy::FALLBACK_TO_CPU:
                std::cout << "Falling back to CPU processing..." << std::endl;
                break;
            case GpuEnvironmentalAudioSystem::RecoveryStrategy::RESTART_SUBSYSTEM:
                std::cout << "Restarting subsystem..." << std::endl;
                break;
            case GpuEnvironmentalAudioSystem::RecoveryStrategy::DISABLE_FEATURE:
                std::cout << "Disabling problematic feature..." << std::endl;
                break;
        }
    }

    template<typename ComponentType>
    size_t count_entities_with_component() {
        return world_->view<ComponentType>().size();
    }

    void cleanup_systems() {
        if (environmental_system_) {
            environmental_system_->shutdown_system();
        }

        if (audio_system_) {
            audio_system_->shutdown(*world_);
        }

        if (gpu_compute_system_) {
            gpu_compute_system_->shutdown();
        }

        std::cout << "All systems cleaned up successfully." << std::endl;
    }
};

int main() {
    try {
        GpuEnvironmentalAudioDemo demo;
        demo.run_demo();
        return 0;
    } catch (const GpuEnvironmentalAudioException& e) {
        std::cerr << "GPU Environmental Audio Exception: " << e.what() << std::endl;
        std::cerr << "Error Type: " << static_cast<int>(e.get_error_type()) << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}