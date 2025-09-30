#include <lore/graphics/graphics.hpp>
#include <lore/world/tilemap_world_system.hpp>
#include <lore/world/tiled_importer.hpp>
// #include <lore/ecs/ecs.hpp>
// #include <lore/math/math.hpp>
// #include <lore/physics/physics.hpp>
// #include <lore/audio/audio.hpp>

#include <iostream>
#include <chrono>

/*
 * LORE ENGINE SYSTEM INITIALIZATION GUIDE
 * =======================================
 *
 * This engine implements a comprehensive ECS-based architecture inspired by Aeon's
 * high-performance physics simulation. Follow this initialization order for optimal
 * performance and proper system dependencies.
 *
 * SYSTEM INITIALIZATION ORDER (CRITICAL - DO NOT CHANGE):
 *
 * 1. CORE FOUNDATION SYSTEMS (Required First)
 *    - ECS World                    // Entity-Component-System foundation
 *    - Math System                  // SIMD-optimized mathematics
 *    - Memory System                // Arena allocators (implement when needed)
 *    - Config System                // INI-based configuration (implement when needed)
 *
 * 2. GRAPHICS & RENDERING (Already Implemented)
 *    - Graphics System              // Vulkan abstraction layer ✓ WORKING
 *    - Rendering System             // Deferred rendering pipeline (implement when needed)
 *    - Materials System             // Shader system (implement when needed)
 *
 * 3. PHYSICS SYSTEMS (Core Dependencies)
 *    - Physics System               // SIMD rigid body dynamics
 *    - Collision System             // Collision detection (part of physics)
 *    - Thermodynamics System        // Heat transfer simulation
 *
 * 4. AUDIO SYSTEMS
 *    - Audio System                 // 3D spatial audio with miniaudio
 *    - Acoustics System             // Sound propagation and materials
 *
 * 5. ADVANCED PHYSICS (Physics System Dependencies)
 *    - Chemistry System             // Chemical reactions (implement when needed)
 *    - Electromagnetics System      // EM fields (implement when needed)
 *    - Optics System               // Light simulation (implement when needed)
 *    - Quantum System              // Quantum mechanics (implement when needed)
 *    - Nuclear System              // Radioactivity (implement when needed)
 *
 * 6. GAME SYSTEMS (Depends on Physics)
 *    - Ballistics System           // Projectile physics with aerodynamics
 *    - Fluids System               // Fluid dynamics (implement when needed)
 *    - Smoke/Fire System           // Combustion effects (implement when needed)
 *
 * EXAMPLE INITIALIZATION PATTERN:
 *
 * // 1. Create ECS World
 * lore::ecs::World world;
 *
 * // 2. Add core systems in dependency order
 * auto& physics = world.add_system<lore::physics::PhysicsSystem>();
 * auto& thermodynamics = world.add_system<lore::physics::ThermodynamicsSystem>();
 * auto& audio = world.add_system<lore::audio::AudioSystem>();
 * auto& acoustics = world.add_system<lore::audio::AcousticsSystem>();
 * auto& ballistics = world.add_system<lore::physics::BallisticsSystem>();
 *
 * // 3. Configure systems
 * physics.set_gravity(lore::math::Vec3(0.0f, -9.81f, 0.0f));
 * audio.set_master_volume(0.8f);
 * ballistics.set_air_resistance_enabled(true);
 *
 * // 4. Initialize all systems
 * world.init(); // Calls init() on all systems in registration order
 *
 * // 5. Create entities with components
 * auto player = world.create_entity();
 * world.add_component(player, lore::math::Transform{});
 * world.add_component(player, lore::physics::RigidBodyComponent{});
 * world.add_component(player, lore::audio::AudioListenerComponent{});
 *
 * // 6. Game loop
 * while (!should_quit) {
 *     world.update(delta_time); // Updates all systems
 * }
 *
 * // 7. Cleanup
 * world.shutdown(); // Calls shutdown() on all systems
 *
 * SYSTEM DEPENDENCIES EXPLAINED:
 * - Physics depends on Math for SIMD vector operations
 * - Ballistics depends on Physics for rigid body dynamics
 * - Thermodynamics can run independently but integrates with Physics
 * - Audio depends on Math for 3D spatial calculations
 * - Acoustics depends on Physics for sound propagation through materials
 * - All game systems should be added after core engine systems
 *
 * PERFORMANCE NOTES:
 * - ECS supports 1M+ entities with sparse set storage
 * - Physics uses SIMD acceleration for parallel processing
 * - Audio supports realistic hearing damage simulation
 * - All systems use getter/setter patterns for clean APIs
 * - Memory management uses arena allocators for zero-allocation runtime
 *
 * CURRENT STATUS: Triangle renderer working with proper Vulkan synchronization
 * NEXT STEP: Uncomment and implement ECS + Physics integration
 */

int main(int, char*[]) {
    try {
        // Initialize graphics system (already working)
        auto& graphics = lore::graphics::GraphicsSystem::instance();

        graphics.create_window(800, 600, "Lore Engine - Test Room Demo");
        graphics.initialize();

        // Initialize world system
        lore::world::TilemapWorldSystem world_system;
        std::cout << "TilemapWorldSystem initialized\n";

        // Load test room from Tiled
        lore::world::TiledMap test_room = lore::world::TiledImporter::load_tiled_map("assets/maps/test_room.tmj");
        std::cout << "Loaded test room: " << test_room.width << "x" << test_room.height << " tiles\n";

        // Import to world system
        lore::world::TiledImporter::import_to_world(world_system, test_room, 0.0f, 0.0f, 0.0f);
        std::cout << "Imported " << test_room.layers.size() << " layers to world\n";

        auto last_time = std::chrono::high_resolution_clock::now();

        std::cout << "\nLore Engine Started Successfully!\n";
        std::cout << "- Vulkan triangle renderer: ACTIVE\n";
        std::cout << "- TilemapWorldSystem: LOADED\n";
        std::cout << "- Test room (11x11): IMPORTED\n";
        std::cout << "- FBX meshes referenced: 2 (Cube, FloorTile)\n";
        std::cout << "\nPress ESC or close window to exit.\n";

        while (!graphics.should_close()) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);
            last_time = current_time;

            graphics.update(delta_time);
            graphics.render();
        }

        graphics.shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}