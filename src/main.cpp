#include <lore/graphics/graphics.hpp>
#include <iostream>
#include <chrono>

int main(int, char*[]) {
    try {
        auto& graphics = lore::graphics::GraphicsSystem::instance();

        graphics.create_window(800, 600, "Lore Engine - Classic Triangle");
        graphics.initialize();

        auto last_time = std::chrono::high_resolution_clock::now();

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