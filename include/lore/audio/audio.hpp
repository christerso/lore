#pragma once

#include <lore/math/math.hpp>
#include <lore/ecs/ecs.hpp>
#include <miniaudio.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace lore::audio {

// Advanced 3D spatial audio system with realistic acoustics
// Inspired by Aeon's hearing damage and acoustic material simulation

// Forward declarations
class AudioEngine;
class AudioSource;
class AudioListener;

// Internal audio source data structure for runtime audio processing
struct AudioSourceData {
    bool is_playing = false;
    bool is_paused = false;
    bool was_playing = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    math::Vec3 position{0.0f};
    math::Vec3 velocity{0.0f};
    float distance_attenuation = 1.0f;
    float calculated_left_gain = 0.5f;
    float calculated_right_gain = 0.5f;
    float doppler_pitch_shift = 1.0f;
    mutable float phase = 0.0f; // For test tone generation

    // Getters
    bool get_is_playing() const noexcept { return is_playing; }
    bool get_is_paused() const noexcept { return is_paused; }
    bool get_was_playing() const noexcept { return was_playing; }
    float get_volume() const noexcept { return volume; }
    float get_pitch() const noexcept { return pitch; }
    const math::Vec3& get_position() const noexcept { return position; }
    const math::Vec3& get_velocity() const noexcept { return velocity; }
    float get_distance_attenuation() const noexcept { return distance_attenuation; }
    float get_calculated_left_gain() const noexcept { return calculated_left_gain; }
    float get_calculated_right_gain() const noexcept { return calculated_right_gain; }
    float get_doppler_pitch_shift() const noexcept { return doppler_pitch_shift; }
    float get_phase() const noexcept { return phase; }

    // Setters
    void set_playing(bool playing) noexcept { is_playing = playing; }
    void set_paused(bool paused) noexcept { is_paused = paused; }
    void set_was_playing(bool was_playing_state) noexcept { was_playing = was_playing_state; }
    void set_volume(float vol) noexcept { volume = vol; }
    void set_pitch(float p) noexcept { pitch = p; }
    void set_position(const math::Vec3& pos) noexcept { position = pos; }
    void set_velocity(const math::Vec3& vel) noexcept { velocity = vel; }
    void set_distance_attenuation(float attenuation) noexcept { distance_attenuation = attenuation; }
    void set_calculated_left_gain(float gain) noexcept { calculated_left_gain = gain; }
    void set_calculated_right_gain(float gain) noexcept { calculated_right_gain = gain; }
    void set_doppler_pitch_shift(float shift) noexcept { doppler_pitch_shift = shift; }
    void set_phase(float p) const noexcept { phase = p; }
};

// Audio formats and types
enum class AudioFormat : std::uint8_t {
    Unknown,
    PCM_S16,
    PCM_S24,
    PCM_S32,
    PCM_F32,
    MP3,
    WAV,
    OGG,
    FLAC
};

enum class AudioSourceType : std::uint8_t {
    Static,        // Pre-loaded audio data
    Streaming,     // Streamed from file/network
    Generated      // Procedurally generated
};

// Acoustic material properties for realistic sound propagation
struct AcousticMaterial {
    float absorption_coefficient = 0.1f;  // How much sound is absorbed (0.0 = reflective, 1.0 = absorptive)
    float transmission_coefficient = 0.0f; // How much sound passes through
    float scattering_coefficient = 0.1f;   // How much sound is scattered
    float density = 1000.0f;               // Material density (affects sound speed)
    float impedance = 1.0f;                // Acoustic impedance

    // Getters
    float get_absorption() const noexcept { return absorption_coefficient; }
    float get_transmission() const noexcept { return transmission_coefficient; }
    float get_scattering() const noexcept { return scattering_coefficient; }
    float get_density() const noexcept { return density; }
    float get_impedance() const noexcept { return impedance; }

    // Setters
    void set_absorption(float absorption) noexcept { absorption_coefficient = absorption; }
    void set_transmission(float transmission) noexcept { transmission_coefficient = transmission; }
    void set_scattering(float scattering) noexcept { scattering_coefficient = scattering; }
    void set_density(float d) noexcept { density = d; }
    void set_impedance(float imp) noexcept { impedance = imp; }
};

// Audio source component for ECS
struct AudioSourceComponent {
    std::string audio_file;
    AudioSourceType source_type = AudioSourceType::Static;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;  // -1.0 = left, 0.0 = center, 1.0 = right
    bool is_looping = false;
    bool is_playing = false;
    bool is_paused = false;
    bool is_3d = true;
    float min_distance = 1.0f;    // Distance where volume is at maximum
    float max_distance = 100.0f;  // Distance where volume reaches minimum
    float rolloff_factor = 1.0f;  // How quickly volume decreases with distance
    math::Vec3 velocity{0.0f};    // For Doppler effect calculation

    // Getters
    const std::string& get_audio_file() const noexcept { return audio_file; }
    AudioSourceType get_source_type() const noexcept { return source_type; }
    float get_volume() const noexcept { return volume; }
    float get_pitch() const noexcept { return pitch; }
    float get_pan() const noexcept { return pan; }
    bool get_is_looping() const noexcept { return is_looping; }
    bool get_is_playing() const noexcept { return is_playing; }
    bool get_is_paused() const noexcept { return is_paused; }
    bool get_is_3d() const noexcept { return is_3d; }
    float get_min_distance() const noexcept { return min_distance; }
    float get_max_distance() const noexcept { return max_distance; }
    float get_rolloff_factor() const noexcept { return rolloff_factor; }
    const math::Vec3& get_velocity() const noexcept { return velocity; }

    // Setters
    void set_audio_file(const std::string& file) noexcept { audio_file = file; }
    void set_source_type(AudioSourceType type) noexcept { source_type = type; }
    void set_volume(float vol) noexcept { volume = vol; }
    void set_pitch(float p) noexcept { pitch = p; }
    void set_pan(float p) noexcept { pan = p; }
    void set_looping(bool loop) noexcept { is_looping = loop; }
    void set_3d(bool enable_3d) noexcept { is_3d = enable_3d; }
    void set_min_distance(float dist) noexcept { min_distance = dist; }
    void set_max_distance(float dist) noexcept { max_distance = dist; }
    void set_rolloff_factor(float factor) noexcept { rolloff_factor = factor; }
    void set_velocity(const math::Vec3& vel) noexcept { velocity = vel; }

    // Playback control
    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void rewind() noexcept;
};

// Audio listener component (typically attached to camera/player)
struct AudioListenerComponent {
    math::Vec3 velocity{0.0f};  // For Doppler effect
    float gain = 1.0f;          // Master volume multiplier
    bool is_active = true;      // Whether this listener is currently active

    // Getters
    const math::Vec3& get_velocity() const noexcept { return velocity; }
    float get_gain() const noexcept { return gain; }
    bool get_is_active() const noexcept { return is_active; }

    // Setters
    void set_velocity(const math::Vec3& vel) noexcept { velocity = vel; }
    void set_gain(float g) noexcept { gain = g; }
    void set_active(bool active) noexcept { is_active = active; }
};

// Hearing damage component for realistic hearing simulation
struct HearingComponent {
    float hearing_threshold = 0.0f;        // dB - minimum audible sound level
    float pain_threshold = 120.0f;         // dB - level that causes pain
    float damage_threshold = 85.0f;        // dB - level that causes hearing damage over time
    float temporary_threshold_shift = 0.0f; // Temporary hearing loss
    float permanent_threshold_shift = 0.0f; // Permanent hearing loss
    float exposure_time = 0.0f;            // Cumulative exposure time
    std::vector<float> frequency_response; // Hearing response per frequency band

    // Getters
    float get_hearing_threshold() const noexcept { return hearing_threshold; }
    float get_pain_threshold() const noexcept { return pain_threshold; }
    float get_damage_threshold() const noexcept { return damage_threshold; }
    float get_temporary_threshold_shift() const noexcept { return temporary_threshold_shift; }
    float get_permanent_threshold_shift() const noexcept { return permanent_threshold_shift; }
    float get_exposure_time() const noexcept { return exposure_time; }

    // Setters
    void set_hearing_threshold(float threshold) noexcept { hearing_threshold = threshold; }
    void set_pain_threshold(float threshold) noexcept { pain_threshold = threshold; }
    void set_damage_threshold(float threshold) noexcept { damage_threshold = threshold; }

    // Hearing damage simulation
    void add_exposure(float sound_level_db, float duration_seconds) noexcept;
    float calculate_perceived_volume(float actual_volume, float frequency) const noexcept;
    bool is_hearing_damaged() const noexcept;
};

// Reverb zone component for environmental audio effects
struct ReverbComponent {
    float room_size = 0.5f;          // 0.0 = small room, 1.0 = large hall
    float damping = 0.5f;            // High frequency absorption
    float wet_level = 0.3f;          // Reverb volume
    float dry_level = 0.7f;          // Direct sound volume
    float pre_delay = 0.02f;         // Time before reverb starts (seconds)
    float decay_time = 1.5f;         // Time for reverb to decay to -60dB
    AcousticMaterial wall_material;   // Properties of surrounding surfaces

    // Getters
    float get_room_size() const noexcept { return room_size; }
    float get_damping() const noexcept { return damping; }
    float get_wet_level() const noexcept { return wet_level; }
    float get_dry_level() const noexcept { return dry_level; }
    float get_pre_delay() const noexcept { return pre_delay; }
    float get_decay_time() const noexcept { return decay_time; }
    const AcousticMaterial& get_wall_material() const noexcept { return wall_material; }

    // Setters
    void set_room_size(float size) noexcept { room_size = size; }
    void set_damping(float d) noexcept { damping = d; }
    void set_wet_level(float wet) noexcept { wet_level = wet; }
    void set_dry_level(float dry) noexcept { dry_level = dry; }
    void set_pre_delay(float delay) noexcept { pre_delay = delay; }
    void set_decay_time(float decay) noexcept { decay_time = decay; }
    void set_wall_material(const AcousticMaterial& material) noexcept { wall_material = material; }
};

// Main audio system for ECS integration
class AudioSystem : public ecs::System {
public:
    AudioSystem();
    ~AudioSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Engine configuration
    void set_master_volume(float volume) noexcept;
    float get_master_volume() const noexcept;
    void set_sound_speed(float speed) noexcept;  // Speed of sound in m/s (default: 343.0)
    float get_sound_speed() const noexcept;
    void set_doppler_factor(float factor) noexcept; // Doppler effect strength
    float get_doppler_factor() const noexcept;

    // Audio loading and management
    bool load_audio_file(const std::string& file_path, const std::string& alias = "") noexcept;
    void unload_audio_file(const std::string& alias) noexcept;
    bool is_audio_loaded(const std::string& alias) const noexcept;

    // Global audio effects
    void set_global_reverb(const ReverbComponent& reverb) noexcept;
    void disable_global_reverb() noexcept;
    bool is_global_reverb_enabled() const noexcept;

    // Environmental audio
    void set_ambient_sound_level(float db_level) noexcept;
    float get_ambient_sound_level() const noexcept;
    void set_acoustic_medium(const AcousticMaterial& medium) noexcept; // Air, water, etc.
    const AcousticMaterial& get_acoustic_medium() const noexcept;

    // Audio analysis
    std::vector<float> get_frequency_spectrum() const noexcept; // Returns FFT of current audio mix
    float get_current_peak_level() const noexcept;             // Current peak audio level in dB
    float get_current_rms_level() const noexcept;              // Current RMS audio level in dB

    // Callbacks
    using AudioEventCallback = std::function<void(ecs::EntityHandle, const std::string&)>;
    void set_audio_finished_callback(AudioEventCallback callback) noexcept;
    void set_audio_looped_callback(AudioEventCallback callback) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Acoustics system for realistic sound propagation
class AcousticsSystem : public ecs::System {
public:
    AcousticsSystem();
    ~AcousticsSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Sound propagation settings
    void set_ray_tracing_enabled(bool enabled) noexcept;
    bool is_ray_tracing_enabled() const noexcept;
    void set_max_reflection_bounces(int bounces) noexcept;
    int get_max_reflection_bounces() const noexcept;
    void set_sound_occlusion_enabled(bool enabled) noexcept;
    bool is_sound_occlusion_enabled() const noexcept;

    // Environmental effects
    void add_sound_barrier(const math::geometry::Plane& barrier, const AcousticMaterial& material) noexcept;
    void remove_sound_barrier(int barrier_id) noexcept;
    void clear_sound_barriers() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Utility functions for audio processing
namespace utils {
    // Decibel conversions
    float linear_to_db(float linear_volume) noexcept;
    float db_to_linear(float db_volume) noexcept;

    // Frequency analysis
    std::vector<float> calculate_fft(const std::vector<float>& audio_data) noexcept;
    float calculate_fundamental_frequency(const std::vector<float>& audio_data, float sample_rate) noexcept;

    // Distance attenuation models
    float inverse_distance_attenuation(float distance, float min_distance, float max_distance) noexcept;
    float linear_distance_attenuation(float distance, float min_distance, float max_distance) noexcept;
    float exponential_distance_attenuation(float distance, float min_distance, float rolloff_factor) noexcept;

    // Doppler effect calculation
    float calculate_doppler_shift(const math::Vec3& source_velocity, const math::Vec3& listener_velocity,
                                  const math::Vec3& source_to_listener, float sound_speed, float frequency) noexcept;

    // Hearing damage calculations (based on OSHA standards)
    float calculate_exposure_time_limit(float sound_level_db) noexcept;
    float calculate_noise_dose(float sound_level_db, float exposure_time_hours) noexcept;
    bool is_hearing_protection_required(float sound_level_db) noexcept;
}

} // namespace lore::audio