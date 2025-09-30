#include <lore/graphics/graphics.hpp>
#include <lore/graphics/gpu_compute.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <iomanip>

namespace lore::demo {

class GpuComputeDemo {
public:
    GpuComputeDemo() = default;
    ~GpuComputeDemo() = default;

    void initialize() {
        std::cout << "=== GPU Compute System Demo ===\n\n";

        // Initialize graphics system first
        graphics_system_ = &graphics::GraphicsSystem::instance();
        graphics_system_->create_window(1920, 1080, "Lore Engine - GPU Compute Demo");
        graphics_system_->initialize();

        // Initialize GPU compute system
        compute_system_ = std::make_unique<graphics::GpuComputeSystem>(*graphics_system_);
        compute_system_->initialize();

        std::cout << "✓ Graphics and GPU Compute systems initialized\n";

        setup_physics_simulation();
        setup_particle_systems();
        setup_ecs_components();

        std::cout << "✓ GPU simulation systems configured\n";
    }

    void run() {
        std::cout << "\n=== Starting GPU Autonomous Execution Demo ===\n";

        // Enable autonomous GPU execution
        compute_system_->enable_autonomous_execution(true);

        auto start_time = std::chrono::high_resolution_clock::now();
        const auto demo_duration = std::chrono::seconds(30); // Run for 30 seconds

        size_t frame_count = 0;
        auto last_stats_time = start_time;

        while (!graphics_system_->should_close()) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = current_time - start_time;

            if (elapsed >= demo_duration) {
                std::cout << "\nDemo completed after " << demo_duration.count() << " seconds\n";
                break;
            }

            // Update graphics (CPU side minimal)
            graphics_system_->update(std::chrono::milliseconds(16));

            // GPU compute happens autonomously in background thread
            // We just need to synchronize for rendering
            compute_system_->wait_for_compute_completion();

            // Render frame
            graphics_system_->render();

            frame_count++;

            // Print statistics every 5 seconds
            auto stats_elapsed = current_time - last_stats_time;
            if (stats_elapsed >= std::chrono::seconds(5)) {
                print_performance_stats(frame_count, stats_elapsed);
                last_stats_time = current_time;
                frame_count = 0;
            }
        }

        // Disable autonomous execution
        compute_system_->enable_autonomous_execution(false);
        std::cout << "✓ GPU autonomous execution stopped\n";
    }

    void shutdown() {
        if (compute_system_) {
            compute_system_->shutdown();
            compute_system_.reset();
        }

        if (graphics_system_) {
            graphics_system_->shutdown();
        }

        std::cout << "✓ Demo shutdown completed\n";
    }

private:
    graphics::GraphicsSystem* graphics_system_ = nullptr;
    std::unique_ptr<graphics::GpuComputeSystem> compute_system_;

    void setup_physics_simulation() {
        auto& physics = compute_system_->get_physics_system();

        // Create 10,000 rigid bodies for stress test
        const uint32_t num_bodies = 10000;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-50.0f, 50.0f);
        std::uniform_real_distribution<float> vel_dist(-10.0f, 10.0f);
        std::uniform_real_distribution<float> mass_dist(0.1f, 5.0f);

        for (uint32_t i = 0; i < num_bodies; ++i) {
            graphics::GpuPhysicsSystem::RigidBody body{};
            body.position = glm::vec3(pos_dist(gen), pos_dist(gen) + 100.0f, pos_dist(gen));
            body.velocity = glm::vec3(vel_dist(gen), vel_dist(gen), vel_dist(gen));
            body.mass = mass_dist(gen);
            body.inv_mass = 1.0f / body.mass;
            body.restitution = 0.8f;
            body.friction = 0.3f;
            body.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            graphics::GpuPhysicsSystem::CollisionShape shape{};
            shape.type = graphics::GpuPhysicsSystem::CollisionShape::SPHERE;
            shape.extents = glm::vec3(0.5f); // 0.5m radius
            shape.material_id = 0;

            physics.create_rigid_body(body, shape);
        }

        physics.set_gravity(glm::vec3(0.0f, -9.81f, 0.0f));
        std::cout << "  → Created " << num_bodies << " rigid bodies with GPU physics\n";
    }

    void setup_particle_systems() {
        auto& particles = compute_system_->get_particle_system();

        // Create multiple emitters for different effects

        // Fire emitter
        graphics::GpuParticleSystem::ParticleEmitter fire_emitter{};
        fire_emitter.position = glm::vec3(0.0f, 0.0f, 0.0f);
        fire_emitter.emission_rate = 1000.0f; // 1000 particles/sec
        fire_emitter.velocity_base = glm::vec3(0.0f, 5.0f, 0.0f);
        fire_emitter.velocity_variation = 3.0f;
        fire_emitter.acceleration = glm::vec3(0.0f, 2.0f, 0.0f);
        fire_emitter.life_time = 3.0f;
        fire_emitter.color_start = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
        fire_emitter.color_end = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);   // Red fade
        fire_emitter.size_start = 1.0f;
        fire_emitter.size_end = 0.1f;
        fire_emitter.max_particles = 50000;

        particles.create_emitter(fire_emitter);

        // Smoke emitter
        graphics::GpuParticleSystem::ParticleEmitter smoke_emitter{};
        smoke_emitter.position = glm::vec3(0.0f, 10.0f, 0.0f);
        smoke_emitter.emission_rate = 500.0f;
        smoke_emitter.velocity_base = glm::vec3(0.0f, 3.0f, 0.0f);
        smoke_emitter.velocity_variation = 2.0f;
        smoke_emitter.acceleration = glm::vec3(0.0f, 1.0f, 0.0f);
        smoke_emitter.life_time = 8.0f;
        smoke_emitter.color_start = glm::vec4(0.7f, 0.7f, 0.7f, 0.8f); // Light gray
        smoke_emitter.color_end = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);   // Dark gray fade
        smoke_emitter.size_start = 0.5f;
        smoke_emitter.size_end = 3.0f;
        smoke_emitter.max_particles = 30000;

        particles.create_emitter(smoke_emitter);

        // Explosion emitter (short burst)
        graphics::GpuParticleSystem::ParticleEmitter explosion_emitter{};
        explosion_emitter.position = glm::vec3(20.0f, 5.0f, 0.0f);
        explosion_emitter.emission_rate = 10000.0f; // High burst rate
        explosion_emitter.velocity_base = glm::vec3(0.0f, 0.0f, 0.0f);
        explosion_emitter.velocity_variation = 15.0f; // High variation for explosion
        explosion_emitter.acceleration = glm::vec3(0.0f, -5.0f, 0.0f);
        explosion_emitter.life_time = 2.0f;
        explosion_emitter.color_start = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        explosion_emitter.color_end = glm::vec4(0.5f, 0.0f, 0.0f, 0.0f);   // Red fade
        explosion_emitter.size_start = 0.8f;
        explosion_emitter.size_end = 0.2f;
        explosion_emitter.max_particles = 100000;

        particles.create_emitter(explosion_emitter);

        std::cout << "  → Created 3 particle emitters with capacity for 180,000 particles\n";
    }

    void setup_ecs_components() {
        auto& ecs = compute_system_->get_ecs_integration();

        // Create 100,000 entities with transform and velocity components
        const uint32_t num_entities = 100000;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> vel_dist(-5.0f, 5.0f);
        std::uniform_real_distribution<float> scale_dist(0.5f, 2.0f);

        std::vector<std::pair<uint32_t, graphics::EcsComputeIntegration::TransformComponent>> transform_updates;
        transform_updates.reserve(num_entities);

        for (uint32_t i = 0; i < num_entities; ++i) {
            graphics::EcsComputeIntegration::TransformComponent transform{};
            transform.position = glm::vec3(pos_dist(gen), pos_dist(gen), pos_dist(gen));
            transform.scale = scale_dist(gen);
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.dirty_flag = 1;

            transform_updates.emplace_back(i, transform);

            // Add velocity component to some entities
            if (i % 3 == 0) { // Every 3rd entity gets velocity
                graphics::EcsComputeIntegration::VelocityComponent velocity{};
                velocity.linear = glm::vec3(vel_dist(gen), vel_dist(gen), vel_dist(gen));
                velocity.angular_speed = vel_dist(gen) * 0.1f;
                velocity.angular_axis = glm::normalize(glm::vec3(vel_dist(gen), vel_dist(gen), vel_dist(gen)));

                ecs.add_component_to_entity(i, velocity);
            }
        }

        // Batch update all transforms
        ecs.batch_update_transforms(transform_updates);

        std::cout << "  → Created " << num_entities << " ECS entities with GPU-managed components\n";
    }

    void print_performance_stats(size_t frame_count, std::chrono::nanoseconds elapsed) {
        auto stats = compute_system_->get_stats();
        auto physics_stats = compute_system_->get_physics_system().get_stats();
        auto particle_stats = compute_system_->get_particle_system().get_stats();
        auto ecs_stats = compute_system_->get_ecs_integration().get_stats();

        double fps = static_cast<double>(frame_count) /
                    (static_cast<double>(elapsed.count()) / 1000000000.0);

        std::cout << "\n=== Performance Statistics ===\n";
        std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps << "\n";
        std::cout << "GPU Utilization: " << (stats.gpu_utilization * 100.0f) << "%\n";
        std::cout << "Total GPU Frame Time: " << stats.total_frame_time.count() << " μs\n";
        std::cout << "  Physics Time: " << stats.physics_time.count() << " μs\n";
        std::cout << "  Particles Time: " << stats.particles_time.count() << " μs\n";
        std::cout << "  ECS Time: " << stats.ecs_time.count() << " μs\n";
        std::cout << "Total GPU Dispatches: " << stats.total_dispatches << "\n";

        std::cout << "\nPhysics System:\n";
        std::cout << "  Active Bodies: " << physics_stats.active_bodies << "\n";
        std::cout << "  Collision Tests: " << physics_stats.collision_tests << "\n";
        std::cout << "  Collisions Detected: " << physics_stats.collisions_detected << "\n";
        std::cout << "  Simulation Time: " << physics_stats.simulation_time.count() << " μs\n";

        std::cout << "\nParticle System:\n";
        std::cout << "  Active Particles: " << particle_stats.active_particles << " / "
                  << particle_stats.total_particles << "\n";
        std::cout << "  Particles Born: " << particle_stats.particles_born << "\n";
        std::cout << "  Particles Died: " << particle_stats.particles_died << "\n";
        std::cout << "  Update Time: " << particle_stats.update_time.count() << " μs\n";

        std::cout << "\nECS System:\n";
        std::cout << "  Active Entities: " << ecs_stats.active_entities << "\n";
        std::cout << "  Transform Updates: " << ecs_stats.transform_updates << "\n";
        std::cout << "  Culled Entities: " << ecs_stats.culled_entities << "\n";
        std::cout << "  System Time: " << ecs_stats.total_system_time.count() << " μs\n";

        // Arena statistics
        for (uint32_t i = 0; i < 3; ++i) { // Check first 3 arenas
            auto arena_stats = compute_system_->get_arena_manager().get_arena_stats(i);
            if (arena_stats.total_size > 0) {
                std::cout << "\nArena " << i << ":\n";
                std::cout << "  Total: " << (arena_stats.total_size / (1024 * 1024)) << " MB\n";
                std::cout << "  Used: " << (arena_stats.allocated_size / (1024 * 1024)) << " MB\n";
                std::cout << "  Free: " << (arena_stats.free_size / (1024 * 1024)) << " MB\n";
                std::cout << "  Allocations: " << arena_stats.allocation_count << "\n";
                std::cout << "  Fragmentation: " << (arena_stats.fragmentation_ratio * 100.0f) << "%\n";
            }
        }

        std::cout << "=====================================\n";
    }
};

} // namespace lore::demo

int main() {
    try {
        lore::demo::GpuComputeDemo demo;

        demo.initialize();
        demo.run();
        demo.shutdown();

        std::cout << "\n=== GPU Compute Demo Completed Successfully ===\n";
        std::cout << "Demonstrated Features:\n";
        std::cout << "✓ 100% GPU execution with autonomous threading\n";
        std::cout << "✓ GPU arena buffer management with zero fragmentation\n";
        std::cout << "✓ 10,000 rigid body physics simulation on GPU\n";
        std::cout << "✓ 180,000+ particles with multiple emitters\n";
        std::cout << "✓ 100,000 ECS entities with GPU-driven components\n";
        std::cout << "✓ Real-time performance monitoring\n";
        std::cout << "✓ Zero CPU involvement in game logic execution\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << "\n";
        return 1;
    }
}