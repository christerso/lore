#pragma once

#include <string>
#include <chrono>

namespace lore::graphics {

class GraphicsSystem {
public:
    static GraphicsSystem& instance();

    // Window management
    void create_window(int width, int height, const std::string& title);
    bool should_close() const;

    // Graphics lifecycle
    void initialize();
    void update(std::chrono::milliseconds delta_time);
    void render();
    void shutdown();

    // Prevent copying
    GraphicsSystem(const GraphicsSystem&) = delete;
    GraphicsSystem& operator=(const GraphicsSystem&) = delete;

private:
    GraphicsSystem() = default;
    ~GraphicsSystem() = default;

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace lore::graphics