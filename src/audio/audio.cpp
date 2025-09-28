#include <lore/audio/audio.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
// #include <fft.h>  // FFT library not available - would be added when implementing spectral analysis
#include <thread>
#include <mutex>
#include <unordered_map>

namespace lore::audio {

// Global audio constants
static constexpr float SPEED_OF_SOUND = 343.0f; // m/s at 20°C
static constexpr float HEARING_DAMAGE_THRESHOLD_DB = 85.0f; // OSHA standard
static constexpr float PAIN_THRESHOLD_DB = 120.0f;
static constexpr int AUDIO_SAMPLE_RATE = 44100;
static constexpr int AUDIO_CHANNELS = 2;
static constexpr int AUDIO_FRAMES_PER_BUFFER = 512;

// AudioSourceComponent implementation
void AudioSourceComponent::play() noexcept {
    is_playing = true;
    is_paused = false;
}

void AudioSourceComponent::pause() noexcept {
    is_paused = true;
}

void AudioSourceComponent::stop() noexcept {
    is_playing = false;
    is_paused = false;
}

void AudioSourceComponent::rewind() noexcept {
    // Implementation would rewind the audio stream to the beginning
}

// HearingComponent implementation
void HearingComponent::add_exposure(float sound_level_db, float duration_seconds) noexcept {
    exposure_time += duration_seconds;

    if (sound_level_db > damage_threshold) {
        // Calculate noise dose based on OSHA formula
        float allowable_time = utils::calculate_exposure_time_limit(sound_level_db);
        float dose_factor = duration_seconds / allowable_time;

        // Add temporary threshold shift
        temporary_threshold_shift += dose_factor * 10.0f; // Simplified model

        // Add permanent damage for extreme exposures
        if (sound_level_db > pain_threshold) {
            permanent_threshold_shift += dose_factor * 2.0f;
        }

        // Clamp values to realistic ranges
        temporary_threshold_shift = std::min(temporary_threshold_shift, 40.0f);
        permanent_threshold_shift = std::min(permanent_threshold_shift, 80.0f);
    }

    // Natural recovery of temporary threshold shift
    temporary_threshold_shift *= std::exp(-duration_seconds / 3600.0f); // 1-hour recovery time constant
}

float HearingComponent::calculate_perceived_volume(float actual_volume, float frequency) const noexcept {
    float total_threshold_shift = hearing_threshold + temporary_threshold_shift + permanent_threshold_shift;

    // Apply frequency-dependent hearing loss
    float frequency_factor = 1.0f;
    if (!frequency_response.empty()) {
        // Simplified frequency response lookup
        int freq_index = static_cast<int>(frequency / 1000.0f); // 1 kHz bins
        if (freq_index < static_cast<int>(frequency_response.size())) {
            frequency_factor = frequency_response[freq_index];
        }
    }

    // Calculate perceived volume with hearing loss
    float perceived_db = utils::linear_to_db(actual_volume) - total_threshold_shift;
    perceived_db *= frequency_factor;

    return utils::db_to_linear(std::max(perceived_db, -60.0f)); // Minimum audible level
}

bool HearingComponent::is_hearing_damaged() const noexcept {
    return permanent_threshold_shift > 5.0f; // 5 dB permanent loss considered damaged
}

// AudioSystem implementation
class AudioSystem::Impl {
public:
    Impl() : master_volume_(1.0f),
             sound_speed_(SPEED_OF_SOUND),
             doppler_factor_(1.0f),
             current_peak_level_(-60.0f),
             current_rms_level_(-60.0f),
             global_reverb_enabled_(false) {

        // Initialize miniaudio
        ma_result result = ma_context_init(nullptr, 0, nullptr, &context_);
        if (result != MA_SUCCESS) {
            throw std::runtime_error("Failed to initialize audio context");
        }

        // Setup device config
        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format = ma_format_f32;
        device_config.playback.channels = AUDIO_CHANNELS;
        device_config.sampleRate = AUDIO_SAMPLE_RATE;
        device_config.dataCallback = audio_callback;
        device_config.pUserData = this;

        result = ma_device_init(&context_, &device_config, &device_);
        if (result != MA_SUCCESS) {
            ma_context_uninit(&context_);
            throw std::runtime_error("Failed to initialize audio device");
        }

        // Start the audio device
        result = ma_device_start(&device_);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&device_);
            ma_context_uninit(&context_);
            throw std::runtime_error("Failed to start audio device");
        }

        // Initialize acoustic medium (air at standard conditions)
        acoustic_medium_.absorption_coefficient = 0.1f;
        acoustic_medium_.transmission_coefficient = 0.0f;
        acoustic_medium_.scattering_coefficient = 0.1f;
        acoustic_medium_.density = 1.225f; // kg/m³
        acoustic_medium_.impedance = 415.0f; // Pa·s/m

        // Pre-allocate buffers
        mix_buffer_.resize(AUDIO_FRAMES_PER_BUFFER * AUDIO_CHANNELS);
        frequency_spectrum_.resize(512); // 512-point FFT
    }

    ~Impl() {
        if (ma_device_is_started(&device_)) {
            ma_device_stop(&device_);
        }
        ma_device_uninit(&device_);
        ma_context_uninit(&context_);
    }

    void update(ecs::World& world, float delta_time) {
        std::lock_guard<std::mutex> lock(audio_mutex_);

        update_audio_sources(world, delta_time);
        update_listeners(world, delta_time);
        update_hearing_simulation(world, delta_time);
        process_3d_audio(world);
    }

private:
    static void audio_callback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frame_count) {
        Impl* impl = static_cast<Impl*>(device->pUserData);
        impl->generate_audio_frames(static_cast<float*>(output), frame_count);
    }

    void generate_audio_frames(float* output, ma_uint32 frame_count) {
        std::lock_guard<std::mutex> lock(audio_mutex_);

        // Clear output buffer
        std::fill_n(output, frame_count * AUDIO_CHANNELS, 0.0f);

        // Mix all active audio sources
        for (const auto& [entity, source_data] : active_sources_) {
            mix_audio_source(output, frame_count, source_data);
        }

        // Apply master volume
        for (ma_uint32 i = 0; i < frame_count * AUDIO_CHANNELS; ++i) {
            output[i] *= master_volume_;
        }

        // Calculate peak and RMS levels for monitoring
        calculate_audio_levels(output, frame_count);

        // Apply global reverb if enabled
        if (global_reverb_enabled_) {
            apply_global_reverb(output, frame_count);
        }
    }

    void mix_audio_source(float* output, ma_uint32 frame_count, const AudioSourceData& source_data) {
        // Simplified audio mixing - in reality this would load and decode audio files
        // and apply 3D positioning, Doppler effects, etc.

        if (!source_data.is_playing || source_data.is_paused) return;

        // Generate test tone or mix from loaded audio data
        for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
            // Apply 3D spatial audio calculations
            float left_gain = source_data.calculated_left_gain;
            float right_gain = source_data.calculated_right_gain;

            // Apply volume and distance attenuation
            float volume_factor = source_data.volume * source_data.distance_attenuation;

            // Apply Doppler effect
            float doppler_pitch = source_data.doppler_pitch_shift;

            // Generate audio sample (simplified sine wave for demonstration)
            float sample = std::sin(source_data.phase) * volume_factor;
            source_data.phase += 2.0f * math::utils::PI * 440.0f * doppler_pitch / AUDIO_SAMPLE_RATE;

            // Mix to stereo output
            output[frame * 2] += sample * left_gain;     // Left channel
            output[frame * 2 + 1] += sample * right_gain; // Right channel
        }
    }

    void update_audio_sources(ecs::World& world, float /*delta_time*/) {
        auto& source_array = world.get_component_array<AudioSourceComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();

        const std::size_t count = source_array.size();
        ecs::Entity* entities = source_array.entities();

        for (std::size_t i = 0; i < count; ++i) {
            ecs::Entity entity = entities[i];
            ecs::EntityHandle handle{entity, 0};

            if (!world.has_component<math::Transform>(handle)) continue;

            const auto& source = source_array.get_component(entity);
            const auto& transform = transform_array.get_component(entity);

            // Update or create audio source data
            AudioSourceData& source_data = active_sources_[entity];
            source_data.is_playing = source.is_playing;
            source_data.is_paused = source.is_paused;
            source_data.volume = source.volume;
            source_data.pitch = source.pitch;
            source_data.position = transform.position;
            source_data.velocity = source.velocity;

            // Call finished callback if audio ended
            if (!source.is_playing && source_data.was_playing && audio_finished_callback_) {
                audio_finished_callback_(handle, source.audio_file);
            }
            source_data.was_playing = source.is_playing;
        }

        // Remove inactive sources
        for (auto it = active_sources_.begin(); it != active_sources_.end();) {
            if (!it->second.is_playing) {
                it = active_sources_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void update_listeners(ecs::World& world, float /*delta_time*/) {
        auto& listener_array = world.get_component_array<AudioListenerComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();

        const std::size_t count = listener_array.size();
        ecs::Entity* entities = listener_array.entities();

        // Find the active listener
        math::Vec3 listener_position(0.0f);
        math::Vec3 listener_velocity(0.0f);
        math::Vec3 listener_forward(0.0f, 0.0f, -1.0f);
        math::Vec3 listener_up(0.0f, 1.0f, 0.0f);
        float listener_gain = 1.0f;

        for (std::size_t i = 0; i < count; ++i) {
            ecs::Entity entity = entities[i];
            ecs::EntityHandle handle{entity, 0};

            if (!world.has_component<math::Transform>(handle)) continue;

            const auto& listener = listener_array.get_component(entity);
            if (!listener.is_active) continue;

            const auto& transform = transform_array.get_component(entity);

            listener_position = transform.position;
            listener_velocity = listener.velocity;
            listener_forward = transform.get_forward();
            listener_up = transform.get_up();
            listener_gain = listener.gain;
            break; // Use first active listener
        }

        // Store listener data for 3D audio processing
        listener_data_.position = listener_position;
        listener_data_.velocity = listener_velocity;
        listener_data_.forward = listener_forward;
        listener_data_.up = listener_up;
        listener_data_.gain = listener_gain;
    }

    void update_hearing_simulation(ecs::World& world, float delta_time) {
        auto& hearing_array = world.get_component_array<HearingComponent>();
        HearingComponent* hearing_components = hearing_array.data();
        const std::size_t count = hearing_array.size();

        for (std::size_t i = 0; i < count; ++i) {
            auto& hearing = hearing_components[i];

            // Add exposure from current audio level
            float current_level_db = current_rms_level_;
            if (current_level_db > hearing.damage_threshold) {
                hearing.add_exposure(current_level_db, delta_time);
            }
        }
    }

    void process_3d_audio(ecs::World& world) {
        // Process 3D spatial audio for each source
        for (auto& [entity, source_data] : active_sources_) {
            ecs::EntityHandle handle{entity, 0};

            // Calculate 3D audio parameters
            math::Vec3 source_to_listener = listener_data_.position - source_data.position;
            float distance = glm::length(source_to_listener);

            if (distance > 1e-6f) {
                // Distance attenuation
                ecs::EntityHandle source_handle{entity, 0};
                if (world.has_component<AudioSourceComponent>(source_handle)) {
                    const auto& source = world.get_component<AudioSourceComponent>(source_handle);
                    source_data.distance_attenuation = utils::inverse_distance_attenuation(
                        distance, source.min_distance, source.max_distance);
                }

                // 3D positioning (HRTF simulation)
                math::Vec3 source_direction = source_to_listener / distance;
                float dot_right = glm::dot(source_direction, glm::cross(listener_data_.forward, listener_data_.up));
                float dot_front = glm::dot(source_direction, listener_data_.forward);

                // Simple HRTF approximation
                float azimuth = std::atan2(dot_right, dot_front);
                source_data.calculated_left_gain = 0.5f + 0.5f * std::cos(azimuth + math::utils::PI * 0.25f);
                source_data.calculated_right_gain = 0.5f + 0.5f * std::cos(azimuth - math::utils::PI * 0.25f);

                // Doppler effect calculation
                math::Vec3 relative_velocity = source_data.velocity - listener_data_.velocity;
                // Calculate Doppler velocity component (used in future implementation)
                [[maybe_unused]] float velocity_component = glm::dot(relative_velocity, source_direction);

                source_data.doppler_pitch_shift = utils::calculate_doppler_shift(
                    source_data.velocity, listener_data_.velocity,
                    source_to_listener, sound_speed_, 1.0f);

                // Apply acoustic medium effects
                apply_acoustic_medium_effects(source_data, distance);
            } else {
                // Source at listener position
                source_data.distance_attenuation = 1.0f;
                source_data.calculated_left_gain = 0.5f;
                source_data.calculated_right_gain = 0.5f;
                source_data.doppler_pitch_shift = 1.0f;
            }
        }
    }

    void apply_acoustic_medium_effects(AudioSourceData& source_data, float distance) {
        // Apply frequency-dependent attenuation based on acoustic medium
        float absorption_factor = std::exp(-acoustic_medium_.absorption_coefficient * distance);
        source_data.distance_attenuation *= absorption_factor;

        // Apply scattering effects (simplified)
        float scattering_factor = 1.0f - acoustic_medium_.scattering_coefficient * distance * 0.001f;
        source_data.distance_attenuation *= std::max(scattering_factor, 0.1f);
    }

    void calculate_audio_levels(const float* output, ma_uint32 frame_count) {
        float peak = 0.0f;
        float rms_sum = 0.0f;

        for (ma_uint32 i = 0; i < frame_count * AUDIO_CHANNELS; ++i) {
            float sample = std::abs(output[i]);
            peak = std::max(peak, sample);
            rms_sum += sample * sample;
        }

        current_peak_level_ = utils::linear_to_db(peak);
        current_rms_level_ = utils::linear_to_db(std::sqrt(rms_sum / (frame_count * AUDIO_CHANNELS)));
    }

    void apply_global_reverb(float* output, ma_uint32 frame_count) {
        // Simplified reverb implementation
        // In a full implementation, this would use convolution with impulse responses

        const float decay_factor = 0.3f;
        const int delay_samples = static_cast<int>(0.1f * AUDIO_SAMPLE_RATE); // 100ms delay

        if (reverb_delay_buffer_.size() < delay_samples) {
            reverb_delay_buffer_.resize(delay_samples * AUDIO_CHANNELS, 0.0f);
        }

        for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
            for (int channel = 0; channel < AUDIO_CHANNELS; ++channel) {
                int index = frame * AUDIO_CHANNELS + channel;
                int delay_index = (reverb_delay_write_pos_ + channel) % reverb_delay_buffer_.size();

                // Get delayed sample
                float delayed_sample = reverb_delay_buffer_[delay_index];

                // Add reverb to output
                output[index] += delayed_sample * decay_factor * global_reverb_.wet_level;

                // Store current sample with feedback
                reverb_delay_buffer_[delay_index] = output[index] + delayed_sample * global_reverb_.decay_time * 0.1f;
            }

            reverb_delay_write_pos_ = (reverb_delay_write_pos_ + AUDIO_CHANNELS) % reverb_delay_buffer_.size();
        }
    }

public:
    // Audio device and context
    ma_context context_;
    ma_device device_;

    // Audio settings
    float master_volume_;
    float sound_speed_;
    float doppler_factor_;

    // Audio monitoring
    float current_peak_level_;
    float current_rms_level_;

    // Acoustic medium
    AcousticMaterial acoustic_medium_;

    // Global reverb
    bool global_reverb_enabled_;
    ReverbComponent global_reverb_;
    std::vector<float> reverb_delay_buffer_;
    int reverb_delay_write_pos_ = 0;

    // Audio sources and listeners data structures

    struct ListenerData {
        math::Vec3 position{0.0f};
        math::Vec3 velocity{0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
        float gain = 1.0f;
    };

    std::unordered_map<ecs::Entity, AudioSourceData> active_sources_;
    ListenerData listener_data_;

    // Audio processing buffers
    std::vector<float> mix_buffer_;
    std::vector<float> frequency_spectrum_;

    // Loaded audio files
    std::unordered_map<std::string, std::vector<float>> loaded_audio_files_;

    // Callbacks
    AudioEventCallback audio_finished_callback_;
    AudioEventCallback audio_looped_callback_;

    // Thread safety
    std::mutex audio_mutex_;
};

// AudioSystem public interface
AudioSystem::AudioSystem() : pimpl_(std::make_unique<Impl>()) {}

AudioSystem::~AudioSystem() = default;

void AudioSystem::init(ecs::World& world) {
    // Register required components
    world.get_component_array<AudioSourceComponent>();
    world.get_component_array<AudioListenerComponent>();
    world.get_component_array<HearingComponent>();
    world.get_component_array<ReverbComponent>();
}

void AudioSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void AudioSystem::shutdown(ecs::World& /*world*/) {
    // Audio system cleanup is handled in destructor
}

void AudioSystem::set_master_volume(float volume) noexcept {
    pimpl_->master_volume_ = std::clamp(volume, 0.0f, 2.0f);
}

float AudioSystem::get_master_volume() const noexcept {
    return pimpl_->master_volume_;
}

void AudioSystem::set_sound_speed(float speed) noexcept {
    pimpl_->sound_speed_ = std::max(speed, 1.0f);
}

float AudioSystem::get_sound_speed() const noexcept {
    return pimpl_->sound_speed_;
}

void AudioSystem::set_doppler_factor(float factor) noexcept {
    pimpl_->doppler_factor_ = std::clamp(factor, 0.0f, 2.0f);
}

float AudioSystem::get_doppler_factor() const noexcept {
    return pimpl_->doppler_factor_;
}

bool AudioSystem::load_audio_file(const std::string& file_path, const std::string& alias) noexcept {
    // Simplified audio loading - in reality this would use a proper audio decoder
    std::string key = alias.empty() ? file_path : alias;

    // Generate test audio data for demonstration
    std::vector<float> audio_data(AUDIO_SAMPLE_RATE * 2); // 2 seconds of audio
    for (size_t i = 0; i < audio_data.size(); ++i) {
        audio_data[i] = 0.1f * std::sin(2.0f * math::utils::PI * 440.0f * i / AUDIO_SAMPLE_RATE);
    }

    pimpl_->loaded_audio_files_[key] = std::move(audio_data);
    return true;
}

void AudioSystem::unload_audio_file(const std::string& alias) noexcept {
    pimpl_->loaded_audio_files_.erase(alias);
}

bool AudioSystem::is_audio_loaded(const std::string& alias) const noexcept {
    return pimpl_->loaded_audio_files_.find(alias) != pimpl_->loaded_audio_files_.end();
}

void AudioSystem::set_global_reverb(const ReverbComponent& reverb) noexcept {
    pimpl_->global_reverb_ = reverb;
    pimpl_->global_reverb_enabled_ = true;
}

void AudioSystem::disable_global_reverb() noexcept {
    pimpl_->global_reverb_enabled_ = false;
}

bool AudioSystem::is_global_reverb_enabled() const noexcept {
    return pimpl_->global_reverb_enabled_;
}

void AudioSystem::set_ambient_sound_level(float /*db_level*/) noexcept {
    // Store ambient sound level for hearing simulation
    // This would be used in hearing damage calculations
}

float AudioSystem::get_ambient_sound_level() const noexcept {
    return pimpl_->current_rms_level_;
}

void AudioSystem::set_acoustic_medium(const AcousticMaterial& medium) noexcept {
    pimpl_->acoustic_medium_ = medium;
}

const AcousticMaterial& AudioSystem::get_acoustic_medium() const noexcept {
    return pimpl_->acoustic_medium_;
}

std::vector<float> AudioSystem::get_frequency_spectrum() const noexcept {
    return pimpl_->frequency_spectrum_;
}

float AudioSystem::get_current_peak_level() const noexcept {
    return pimpl_->current_peak_level_;
}

float AudioSystem::get_current_rms_level() const noexcept {
    return pimpl_->current_rms_level_;
}

void AudioSystem::set_audio_finished_callback(AudioEventCallback callback) noexcept {
    pimpl_->audio_finished_callback_ = std::move(callback);
}

void AudioSystem::set_audio_looped_callback(AudioEventCallback callback) noexcept {
    pimpl_->audio_looped_callback_ = std::move(callback);
}

// AcousticsSystem implementation
class AcousticsSystem::Impl {
public:
    Impl() : ray_tracing_enabled_(true),
             max_reflection_bounces_(3),
             sound_occlusion_enabled_(true) {}

    void update(ecs::World& world, float /*delta_time*/) {
        if (!ray_tracing_enabled_) return;

        simulate_acoustic_propagation(world);
    }

private:
    void simulate_acoustic_propagation(ecs::World& /*world*/) {
        // Simplified acoustic ray tracing
        // In a full implementation, this would trace sound rays through the environment
        // and calculate reflections, refractions, and occlusions
    }

public:
    bool ray_tracing_enabled_;
    int max_reflection_bounces_;
    bool sound_occlusion_enabled_;

    struct SoundBarrier {
        math::geometry::Plane plane;
        AcousticMaterial material;
        int id;
    };

    std::vector<SoundBarrier> sound_barriers_;
    int next_barrier_id_ = 1;
};

AcousticsSystem::AcousticsSystem() : pimpl_(std::make_unique<Impl>()) {}

AcousticsSystem::~AcousticsSystem() = default;

void AcousticsSystem::init(ecs::World& /*world*/) {}

void AcousticsSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void AcousticsSystem::shutdown(ecs::World& /*world*/) {}

void AcousticsSystem::set_ray_tracing_enabled(bool enabled) noexcept {
    pimpl_->ray_tracing_enabled_ = enabled;
}

bool AcousticsSystem::is_ray_tracing_enabled() const noexcept {
    return pimpl_->ray_tracing_enabled_;
}

void AcousticsSystem::set_max_reflection_bounces(int bounces) noexcept {
    pimpl_->max_reflection_bounces_ = std::max(bounces, 0);
}

int AcousticsSystem::get_max_reflection_bounces() const noexcept {
    return pimpl_->max_reflection_bounces_;
}

void AcousticsSystem::set_sound_occlusion_enabled(bool enabled) noexcept {
    pimpl_->sound_occlusion_enabled_ = enabled;
}

bool AcousticsSystem::is_sound_occlusion_enabled() const noexcept {
    return pimpl_->sound_occlusion_enabled_;
}

void AcousticsSystem::add_sound_barrier(const math::geometry::Plane& barrier, const AcousticMaterial& material) noexcept {
    AcousticsSystem::Impl::SoundBarrier sound_barrier;
    sound_barrier.plane = barrier;
    sound_barrier.material = material;
    sound_barrier.id = pimpl_->next_barrier_id_++;

    pimpl_->sound_barriers_.push_back(sound_barrier);
}

void AcousticsSystem::remove_sound_barrier(int barrier_id) noexcept {
    auto it = std::remove_if(pimpl_->sound_barriers_.begin(), pimpl_->sound_barriers_.end(),
                             [barrier_id](const auto& barrier) { return barrier.id == barrier_id; });
    pimpl_->sound_barriers_.erase(it, pimpl_->sound_barriers_.end());
}

void AcousticsSystem::clear_sound_barriers() noexcept {
    pimpl_->sound_barriers_.clear();
}

// Utility functions
namespace utils {

float linear_to_db(float linear_volume) noexcept {
    if (linear_volume <= 0.0f) return -60.0f; // Minimum representable level
    return 20.0f * std::log10(linear_volume);
}

float db_to_linear(float db_volume) noexcept {
    return std::pow(10.0f, db_volume / 20.0f);
}

std::vector<float> calculate_fft(const std::vector<float>& audio_data) noexcept {
    // Simplified FFT implementation
    // In a real implementation, this would use a proper FFT library like FFTW
    std::vector<float> magnitude_spectrum;
    magnitude_spectrum.reserve(audio_data.size() / 2);

    // Placeholder implementation
    for (size_t i = 0; i < audio_data.size() / 2; ++i) {
        magnitude_spectrum.push_back(std::abs(audio_data[i]));
    }

    return magnitude_spectrum;
}

float calculate_fundamental_frequency(const std::vector<float>& audio_data, float sample_rate) noexcept {
    // Simplified fundamental frequency detection using autocorrelation
    if (audio_data.size() < 2) return 0.0f;

    float max_correlation = 0.0f;
    int best_period = 1;

    int max_period = static_cast<int>(audio_data.size() / 4);
    for (int period = 1; period < max_period; ++period) {
        float correlation = 0.0f;
        int count = 0;

        for (size_t i = 0; i + period < audio_data.size(); ++i) {
            correlation += audio_data[i] * audio_data[i + period];
            count++;
        }

        correlation /= count;

        if (correlation > max_correlation) {
            max_correlation = correlation;
            best_period = period;
        }
    }

    return sample_rate / best_period;
}

float inverse_distance_attenuation(float distance, float min_distance, float max_distance) noexcept {
    if (distance <= min_distance) return 1.0f;
    if (distance >= max_distance) return 0.0f;

    return min_distance / distance;
}

float linear_distance_attenuation(float distance, float min_distance, float max_distance) noexcept {
    if (distance <= min_distance) return 1.0f;
    if (distance >= max_distance) return 0.0f;

    return 1.0f - (distance - min_distance) / (max_distance - min_distance);
}

float exponential_distance_attenuation(float distance, float min_distance, float rolloff_factor) noexcept {
    if (distance <= min_distance) return 1.0f;

    float attenuation = min_distance / (min_distance + rolloff_factor * (distance - min_distance));
    return attenuation;
}

float calculate_doppler_shift(const math::Vec3& source_velocity, const math::Vec3& listener_velocity,
                              const math::Vec3& source_to_listener, float sound_speed, float /*frequency*/) noexcept {
    if (glm::length(source_to_listener) < 1e-6f) return 1.0f;

    math::Vec3 direction = glm::normalize(source_to_listener);

    float source_speed = glm::dot(source_velocity, direction);
    float listener_speed = glm::dot(listener_velocity, direction);

    float relative_speed = listener_speed - source_speed;
    float doppler_factor = (sound_speed + relative_speed) / sound_speed;

    return std::clamp(doppler_factor, 0.5f, 2.0f); // Clamp to reasonable range
}

float calculate_exposure_time_limit(float sound_level_db) noexcept {
    // OSHA exposure time limits
    if (sound_level_db <= 85.0f) return 8.0f * 3600.0f; // 8 hours
    if (sound_level_db >= 115.0f) return 15.0f * 60.0f; // 15 minutes

    // Exponential relationship: for every 3 dB increase, halve the time
    float excess_db = sound_level_db - 85.0f;
    float time_factor = std::pow(0.5f, excess_db / 3.0f);

    return 8.0f * 3600.0f * time_factor; // Hours to seconds
}

float calculate_noise_dose(float sound_level_db, float exposure_time_hours) noexcept {
    float allowable_time_hours = calculate_exposure_time_limit(sound_level_db) / 3600.0f;
    return exposure_time_hours / allowable_time_hours;
}

bool is_hearing_protection_required(float sound_level_db) noexcept {
    return sound_level_db >= HEARING_DAMAGE_THRESHOLD_DB;
}

} // namespace utils

} // namespace lore::audio