#include "lore/ui/environment_debug_panel.hpp"
#include <imgui.h>
#include <vector>
#include <string>

namespace lore::ui {

void EnvironmentDebugPanel::render() {
    if (!visible_) return;

    ImGui::Begin("Environment Controller", &visible_, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::CollapsingHeader("Quick Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        render_preset_buttons();
    }

    if (ImGui::CollapsingHeader("Time of Day", ImGuiTreeNodeFlags_DefaultOpen)) {
        render_time_controls();
    }

    if (ImGui::CollapsingHeader("Post-Processing")) {
        render_post_processing_controls();
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        render_lighting_controls();
    }

    ImGui::End();
}

void EnvironmentDebugPanel::render_preset_buttons() {
    ImGui::Text("Full Presets:");
    ImGui::Separator();

    ImGui::Columns(2, nullptr, false);

    if (ImGui::Button("Mirror's Edge Bright", ImVec2(-1, 30))) {
        controller_.apply_preset("mirrors_edge_bright");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Mirror's Edge Indoor", ImVec2(-1, 30))) {
        controller_.apply_preset("mirrors_edge_indoor");
    }
    ImGui::NextColumn();

    if (ImGui::Button("High Contrast", ImVec2(-1, 30))) {
        controller_.apply_preset("high_contrast");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Low Contrast", ImVec2(-1, 30))) {
        controller_.apply_preset("low_contrast");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Warm Sunset", ImVec2(-1, 30))) {
        controller_.apply_preset("warm_sunset");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Cool Morning", ImVec2(-1, 30))) {
        controller_.apply_preset("cool_morning");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Neutral Noon", ImVec2(-1, 30))) {
        controller_.apply_preset("neutral_noon");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Moody Overcast", ImVec2(-1, 30))) {
        controller_.apply_preset("moody_overcast");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Cinematic Night", ImVec2(-1, 30))) {
        controller_.apply_preset("cinematic_night");
    }
    ImGui::NextColumn();

    if (ImGui::Button("Vibrant Day", ImVec2(-1, 30))) {
        controller_.apply_preset("vibrant_day");
    }

    ImGui::Columns(1);
    ImGui::Spacing();
}

void EnvironmentDebugPanel::render_time_controls() {
    render_quick_time_buttons();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Time slider
    if (ImGui::SliderFloat("Hour of Day", &time_slider_, 0.0f, 24.0f, "%.1f:00")) {
        controller_.set_time_of_day(time_slider_);
    }

    ImGui::SameLine();
    if (ImGui::Button("Set")) {
        controller_.set_time_of_day(time_slider_);
    }

    // Display current time in readable format
    int hours = static_cast<int>(time_slider_);
    int minutes = static_cast<int>((time_slider_ - hours) * 60.0f);
    ImGui::Text("Current: %02d:%02d", hours, minutes);

    ImGui::Spacing();

    // Time progression
    static float time_speed = 0.0f;
    if (ImGui::SliderFloat("Time Speed", &time_speed, 0.0f, 1.0f, "%.3f hours/sec")) {
        // Time speed will be applied in game loop
    }

    if (time_speed > 0.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Running");
    }

    ImGui::Spacing();

    // Transition controls
    ImGui::Text("Smooth Transition:");
    static float target_time = 18.0f;
    static float transition_duration = 5.0f;

    ImGui::SliderFloat("Target Time", &target_time, 0.0f, 24.0f, "%.1f:00");
    ImGui::SliderFloat("Duration (s)", &transition_duration, 1.0f, 30.0f, "%.1f");

    if (ImGui::Button("Transition", ImVec2(-1, 0))) {
        controller_.transition_to_time(target_time, transition_duration);
    }
}

void EnvironmentDebugPanel::render_quick_time_buttons() {
    ImGui::Text("Quick Times:");
    ImGui::Separator();

    ImGui::Columns(4, nullptr, false);

    if (ImGui::Button("Dawn\n(6 AM)", ImVec2(-1, 40))) {
        controller_.apply_dawn();
        time_slider_ = 6.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Morning\n(9 AM)", ImVec2(-1, 40))) {
        controller_.apply_morning();
        time_slider_ = 9.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Noon\n(12 PM)", ImVec2(-1, 40))) {
        controller_.apply_noon();
        time_slider_ = 12.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Afternoon\n(3 PM)", ImVec2(-1, 40))) {
        controller_.apply_afternoon();
        time_slider_ = 15.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Golden Hour\n(6:30 PM)", ImVec2(-1, 40))) {
        controller_.apply_golden_hour();
        time_slider_ = 18.5f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Dusk\n(8 PM)", ImVec2(-1, 40))) {
        controller_.apply_dusk();
        time_slider_ = 20.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Night\n(12 AM)", ImVec2(-1, 40))) {
        controller_.apply_night();
        time_slider_ = 0.0f;
    }
    ImGui::NextColumn();

    if (ImGui::Button("Midnight\n(2 AM)", ImVec2(-1, 40))) {
        controller_.apply_midnight();
        time_slider_ = 2.0f;
    }

    ImGui::Columns(1);
}

void EnvironmentDebugPanel::render_post_processing_controls() {
    auto& pp = controller_.get_post_processing_mut();

    ImGui::Text("Exposure & Brightness");
    ImGui::Separator();

    if (ImGui::SliderFloat("Exposure (EV)", &pp.exposure_ev, -3.0f, 3.0f, "%.2f")) {
        // Auto-applied
    }

    if (ImGui::SliderFloat("Brightness", &pp.brightness, -0.5f, 0.5f, "%.2f")) {
        // Auto-applied
    }

    if (ImGui::SliderFloat("Contrast", &pp.contrast, 0.5f, 2.0f, "%.2f")) {
        // Auto-applied
    }

    ImGui::Spacing();
    ImGui::Text("Color Grading");
    ImGui::Separator();

    if (ImGui::SliderFloat("Temperature", &pp.temperature, -1.0f, 1.0f, "%.2f")) {
        // Negative = cool (blue), Positive = warm (orange)
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Negative = Cool (Blue)\nPositive = Warm (Orange)");
    }

    if (ImGui::SliderFloat("Tint", &pp.tint, -1.0f, 1.0f, "%.2f")) {
        // Negative = green, Positive = magenta
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Negative = Green\nPositive = Magenta");
    }

    if (ImGui::SliderFloat("Saturation", &pp.saturation, 0.0f, 2.0f, "%.2f")) {
        // 0 = grayscale, 1 = normal, 2 = oversaturated
    }

    if (ImGui::SliderFloat("Vibrance", &pp.vibrance, -1.0f, 1.0f, "%.2f")) {
        // Affects less-saturated colors more
    }

    ImGui::Spacing();
    ImGui::Text("Effects");
    ImGui::Separator();

    if (ImGui::SliderFloat("Vignette", &pp.vignette_intensity, 0.0f, 1.0f, "%.2f")) {
        // 0 = none, 1 = strong
    }

    if (ImGui::SliderFloat("AO Intensity", &pp.ao_intensity, 0.0f, 2.0f, "%.2f")) {
        // Ambient occlusion strength
    }

    ImGui::Spacing();

    // Reset button
    if (ImGui::Button("Reset to Neutral", ImVec2(-1, 0))) {
        pp.exposure_ev = 0.0f;
        pp.brightness = 0.0f;
        pp.contrast = 1.0f;
        pp.temperature = 0.0f;
        pp.tint = 0.0f;
        pp.saturation = 1.0f;
        pp.vibrance = 0.0f;
        pp.vignette_intensity = 0.0f;
        pp.ao_intensity = 0.8f;
    }
}

void EnvironmentDebugPanel::render_lighting_controls() {
    auto& lighting = controller_.get_lighting_mut();

    ImGui::Text("Sun & Sky");
    ImGui::Separator();

    if (ImGui::SliderFloat("Sun Intensity", &lighting.sun_intensity_multiplier, 0.0f, 2.0f, "%.2f")) {
        // Auto-applied
    }

    if (ImGui::SliderFloat("Ambient Intensity", &lighting.ambient_intensity_multiplier, 0.0f, 2.0f, "%.2f")) {
        // Auto-applied
    }

    if (ImGui::SliderFloat("Sky Intensity", &lighting.sky_intensity_multiplier, 0.0f, 2.0f, "%.2f")) {
        // Auto-applied
    }

    ImGui::Spacing();
    ImGui::Text("Shadows");
    ImGui::Separator();

    if (ImGui::SliderFloat("Shadow Strength", &lighting.shadow_strength, 0.0f, 1.0f, "%.2f")) {
        // 0 = no shadows, 1 = full
    }

    ImGui::Spacing();
    ImGui::Text("Fog");
    ImGui::Separator();

    if (ImGui::SliderFloat("Fog Density", &lighting.fog_density_multiplier, 0.0f, 5.0f, "%.2f")) {
        // 1.0 = default
    }

    static float fog_color[3] = {0.0f, 0.0f, 0.0f};
    if (ImGui::ColorEdit3("Fog Color Override", fog_color)) {
        lighting.fog_color_override = math::Vec3{fog_color[0], fog_color[1], fog_color[2]};
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        lighting.fog_color_override = math::Vec3{0.0f, 0.0f, 0.0f};
        fog_color[0] = fog_color[1] = fog_color[2] = 0.0f;
    }

    ImGui::Spacing();

    // Reset button
    if (ImGui::Button("Reset Lighting", ImVec2(-1, 0))) {
        lighting.sun_intensity_multiplier = 1.0f;
        lighting.ambient_intensity_multiplier = 1.0f;
        lighting.sky_intensity_multiplier = 1.0f;
        lighting.shadow_strength = 1.0f;
        lighting.fog_density_multiplier = 1.0f;
        lighting.fog_color_override = math::Vec3{0.0f, 0.0f, 0.0f};
    }
}

} // namespace lore::ui