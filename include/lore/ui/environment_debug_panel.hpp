#pragma once

#include "lore/ecs/systems/environment_controller.hpp"

namespace lore::ui {

/**
 * @brief ImGui debug panel for EnvironmentController
 *
 * Real-time tweaking interface for artists/designers to adjust:
 * - Time of day
 * - Post-processing (contrast, colors, exposure)
 * - Lighting (sun, shadows, fog)
 * - Quick presets
 *
 * Usage:
 * @code
 * EnvironmentDebugPanel panel(env_controller);
 *
 * // In render loop
 * panel.render();
 * @endcode
 */
class EnvironmentDebugPanel {
public:
    explicit EnvironmentDebugPanel(ecs::EnvironmentController& controller)
        : controller_(controller) {}

    /**
     * @brief Render ImGui panel
     * Call this every frame in your ImGui rendering code
     */
    void render();

    /**
     * @brief Set panel visibility
     */
    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }

private:
    void render_time_controls();
    void render_post_processing_controls();
    void render_lighting_controls();
    void render_preset_buttons();
    void render_quick_time_buttons();

    ecs::EnvironmentController& controller_;
    bool visible_ = true;

    // UI state
    float time_slider_ = 12.0f;
    int selected_preset_ = 0;
};

} // namespace lore::ui