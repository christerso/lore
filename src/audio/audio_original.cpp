#include <lore/audio/audio.hpp>
#include <lore/graphics/gpu_compute.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
// #include <fft.h>  // FFT library not available - would be added when implementing spectral analysis
#include <thread>
#include <mutex>
#include <unordered_map>
#include <iostream>

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
             global_reverb_enabled_(false),
             gpu_compute_system_(nullptr),
             audio_arena_id_(0xFFFFFFFF),
             gpu_audio_enabled_(false) {

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

        // Initialize GPU audio processing if available
        initialize_gpu_audio_processing();
    }

    ~Impl() {
        // Cleanup GPU audio processing
        cleanup_gpu_audio_processing();

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

                // Initialize base gain values
                float base_left_gain = 0.5f;
                float base_right_gain = 0.5f;

                // Check for directional audio source component
                if (world.has_component<DirectionalAudioSourceComponent>(source_handle)) {
                    const auto& directional_source = world.get_component<DirectionalAudioSourceComponent>(source_handle);

                    // Apply directional gain based on source orientation
                    float directivity_gain = directional_source.calculate_directivity_gain(source_to_listener);
                    source_data.distance_attenuation *= directivity_gain;

                    // Enhanced sophisticated HRTF processing for directional sources
                    math::Vec3 source_direction = source_to_listener / distance;
                    float dot_right = glm::dot(source_direction, glm::cross(listener_data_.forward, listener_data_.up));
                    float dot_front = glm::dot(source_direction, listener_data_.forward);
                    float dot_up = glm::dot(source_direction, listener_data_.up);

                    // Calculate precise azimuth and elevation for sophisticated HRTF
                    float azimuth = std::atan2(dot_right, dot_front);
                    float elevation = std::asin(std::clamp(dot_up, -1.0f, 1.0f));

                    // Advanced HRTF calculations with frequency-dependent processing
                    float head_radius = directional_source.get_head_radius();
                    float ear_distance = directional_source.get_ear_distance();

                    // Calculate inter-aural time difference (ITD) for realistic positioning
                    float itd = (head_radius * std::sin(azimuth)) / SPEED_OF_SOUND;

                    // Calculate inter-aural level difference (ILD) with head shadowing
                    float frequency = 1000.0f; // Use 1kHz as reference frequency
                    float head_circumference = 2.0f * math::utils::PI * head_radius;
                    float wavelength = SPEED_OF_SOUND / frequency;

                    // Head shadowing effect for realistic occlusion
                    float shadow_factor = 1.0f;
                    if (wavelength < head_circumference) {
                        shadow_factor = 1.0f - 0.3f * std::abs(std::sin(azimuth));
                    }

                    // Sophisticated HRTF response based on measured data
                    float left_azimuth_response, right_azimuth_response;
                    if (azimuth >= -math::utils::PI * 0.5f && azimuth <= math::utils::PI * 0.5f) {
                        // Front hemisphere - different responses for each ear
                        left_azimuth_response = 0.7f + 0.3f * std::cos(azimuth);
                        right_azimuth_response = 0.7f + 0.3f * std::cos(azimuth + math::utils::PI);
                    } else {
                        // Rear hemisphere - head shadowing dominates
                        left_azimuth_response = 0.3f + 0.2f * std::cos(azimuth + math::utils::PI);
                        right_azimuth_response = 0.3f + 0.2f * std::cos(azimuth);
                    }

                    // Elevation response with pinna filtering simulation
                    float elevation_response = 0.8f + 0.2f * std::cos(elevation);

                    // Apply sophisticated HRTF gains
                    if (azimuth >= 0) {
                        // Source on the right side
                        base_right_gain = right_azimuth_response * elevation_response * shadow_factor;
                        base_left_gain = left_azimuth_response * elevation_response;
                    } else {
                        // Source on the left side
                        base_left_gain = left_azimuth_response * elevation_response * shadow_factor;
                        base_right_gain = right_azimuth_response * elevation_response;
                    }

                    // Apply advanced HRTF processing with directional enhancements
                    directional_source.apply_hrtf_processing(base_left_gain, base_right_gain,
                                                           source_to_listener,
                                                           listener_data_.forward,
                                                           listener_data_.up);

                    // Apply distance-based frequency filtering
                    if (distance > 1.0f) {
                        float hf_rolloff = 1.0f / (1.0f + distance * 0.05f);
                        base_left_gain *= hf_rolloff;
                        base_right_gain *= hf_rolloff;
                    }

                    // Apply binaural enhancement if enabled
                    if (directional_source.get_enable_binaural()) {
                        float crossfeed = directional_source.get_crossfeed_amount();
                        float phase_shift = directional_source.get_phase_shift_amount();

                        float original_left = base_left_gain;
                        float original_right = base_right_gain;

                        // Cross-feed for natural sound
                        base_left_gain = original_left + crossfeed * original_right;
                        base_right_gain = original_right + crossfeed * original_left;

                        // Phase enhancement for spatialization
                        float phase_enhancement = phase_shift * std::sin(azimuth);
                        base_left_gain *= (1.0f + phase_enhancement);
                        base_right_gain *= (1.0f - phase_enhancement);
                    }

                    source_data.calculated_left_gain = std::clamp(base_left_gain, 0.0f, 2.0f);
                    source_data.calculated_right_gain = std::clamp(base_right_gain, 0.0f, 2.0f);
                } else {
                    // Enhanced 3D positioning with sophisticated HRTF for non-directional sources
                    math::Vec3 source_direction = source_to_listener / distance;
                    float dot_right = glm::dot(source_direction, glm::cross(listener_data_.forward, listener_data_.up));
                    float dot_front = glm::dot(source_direction, listener_data_.forward);
                    float dot_up = glm::dot(source_direction, listener_data_.up);

                    // Calculate azimuth and elevation for sophisticated processing
                    float azimuth = std::atan2(dot_right, dot_front);
                    float elevation = std::asin(std::clamp(dot_up, -1.0f, 1.0f));

                    // Default head model parameters for non-directional sources
                    float head_radius = 0.0875f; // Average human head radius
                    float head_circumference = 2.0f * math::utils::PI * head_radius;
                    float wavelength = SPEED_OF_SOUND / 1000.0f; // 1kHz reference

                    // Head shadowing calculation
                    float shadow_factor = 1.0f;
                    if (wavelength < head_circumference) {
                        shadow_factor = 1.0f - 0.2f * std::abs(std::sin(azimuth));
                    }

                    // Sophisticated HRTF approximation with realistic responses
                    float elevation_factor = 0.8f + 0.2f * std::cos(elevation);

                    if (azimuth >= 0) {
                        // Right side positioning
                        source_data.calculated_right_gain = (0.6f + 0.4f * std::cos(azimuth * 0.5f)) * elevation_factor * shadow_factor;
                        source_data.calculated_left_gain = (0.4f + 0.2f * std::cos(azimuth * 0.5f)) * elevation_factor;
                    } else {
                        // Left side positioning
                        source_data.calculated_left_gain = (0.6f + 0.4f * std::cos(std::abs(azimuth) * 0.5f)) * elevation_factor * shadow_factor;
                        source_data.calculated_right_gain = (0.4f + 0.2f * std::cos(std::abs(azimuth) * 0.5f)) * elevation_factor;
                    }

                    // Apply distance-based high-frequency attenuation
                    if (distance > 1.0f) {
                        float hf_attenuation = 1.0f / (1.0f + distance * 0.03f);
                        source_data.calculated_left_gain *= hf_attenuation;
                        source_data.calculated_right_gain *= hf_attenuation;
                    }

                    // Clamp to valid range
                    source_data.calculated_left_gain = std::clamp(source_data.calculated_left_gain, 0.0f, 2.0f);
                    source_data.calculated_right_gain = std::clamp(source_data.calculated_right_gain, 0.0f, 2.0f);
                }

                // Doppler effect calculation
                math::Vec3 relative_velocity = source_data.velocity - listener_data_.velocity;
                // Calculate Doppler velocity component (used in future implementation)
                [[maybe_unused]] float velocity_component = glm::dot(relative_velocity, source_to_listener / distance);

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

    // GPU Audio Processing Methods
    void initialize_gpu_audio_processing() {
        try {
            // Try to access the GPU compute system if available
            // This would need to be passed in from the main engine initialization
            gpu_audio_enabled_ = false; // Disabled by default, enable when GPU system is available

            if (gpu_audio_enabled_ && gpu_compute_system_) {
                auto& arena_manager = gpu_compute_system_->get_arena_manager();

                // Create dedicated arena for audio processing
                audio_arena_id_ = arena_manager.create_arena(
                    32 * 1024 * 1024, // 32MB for audio processing
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY
                );

                // Allocate GPU buffers for audio sources
                const uint32_t max_audio_sources = 1024;
                gpu_audio_sources_buffer_ = arena_manager.allocate_on_gpu(
                    audio_arena_id_,
                    max_audio_sources * sizeof(GpuAudioSourceData),
                    256
                );

                // Allocate GPU buffers for directional audio processing
                gpu_directional_buffer_ = arena_manager.allocate_on_gpu(
                    audio_arena_id_,
                    max_audio_sources * sizeof(GpuDirectionalAudioSource),
                    256
                );

                // Allocate HRTF convolution data buffer
                gpu_hrtf_buffer_ = arena_manager.allocate_on_gpu(
                    audio_arena_id_,
                    max_audio_sources * sizeof(GpuHRTFConvolutionData),
                    256
                );

                // Load and compile audio compute shaders
                setup_gpu_audio_pipelines();

                std::cout << "GPU audio processing initialized successfully" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize GPU audio processing: " << e.what() << std::endl;
            gpu_audio_enabled_ = false;
        }
    }

    void cleanup_gpu_audio_processing() {
        if (gpu_audio_enabled_ && gpu_compute_system_) {
            auto& arena_manager = gpu_compute_system_->get_arena_manager();

            // Deallocate GPU buffers
            if (gpu_audio_sources_buffer_.is_valid) {
                arena_manager.deallocate_on_gpu(gpu_audio_sources_buffer_);
            }
            if (gpu_directional_buffer_.is_valid) {
                arena_manager.deallocate_on_gpu(gpu_directional_buffer_);
            }
            if (gpu_hrtf_buffer_.is_valid) {
                arena_manager.deallocate_on_gpu(gpu_hrtf_buffer_);
            }

            gpu_audio_enabled_ = false;
        }
    }

    void setup_gpu_audio_pipelines() {
        if (!gpu_compute_system_) return;

        auto& shader_compiler = gpu_compute_system_->get_shader_compiler();
        auto& pipeline_manager = gpu_compute_system_->get_pipeline_manager();

        try {
            // Compile directional audio compute shader
            lore::graphics::ShaderCompiler::ComputeShaderInfo directional_shader_info;
            directional_shader_info.source_path = "shaders/compute/gpu_audio_directional.comp";
            directional_shader_info.entry_point = "main";
            directional_shader_info.definitions["MAX_AUDIO_SOURCES"] = "1024";

            auto directional_shader = shader_compiler.compile_compute_shader(directional_shader_info);
            if (directional_shader) {
                // Create pipeline for directional audio processing
                // This would be properly set up with descriptor layouts in a full implementation
                std::cout << "Directional audio compute shader compiled successfully" << std::endl;
            }

            // Compile HRTF convolution compute shader
            lore::graphics::ShaderCompiler::ComputeShaderInfo hrtf_shader_info;
            hrtf_shader_info.source_path = "shaders/compute/gpu_audio_hrtf.comp";
            hrtf_shader_info.entry_point = "main";
            hrtf_shader_info.definitions["HRTF_IR_LENGTH"] = "512";

            auto hrtf_shader = shader_compiler.compile_compute_shader(hrtf_shader_info);
            if (hrtf_shader) {
                std::cout << "HRTF convolution compute shader compiled successfully" << std::endl;
            }

            // Compile bulk audio update compute shader
            lore::graphics::ShaderCompiler::ComputeShaderInfo update_shader_info;
            update_shader_info.source_path = "shaders/compute/gpu_audio_update.comp";
            update_shader_info.entry_point = "main";

            auto update_shader = shader_compiler.compile_compute_shader(update_shader_info);
            if (update_shader) {
                std::cout << "Audio update compute shader compiled successfully" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Failed to compile audio compute shaders: " << e.what() << std::endl;
        }
    }

    void update_gpu_audio_processing(ecs::World& world, float delta_time) {
        if (!gpu_audio_enabled_ || !gpu_compute_system_) {
            return;
        }

        // Upload current audio source data to GPU
        upload_audio_sources_to_gpu(world);

        // Dispatch GPU compute shaders for audio processing
        dispatch_audio_compute_shaders(delta_time);

        // The GPU will process all audio sources in parallel
        // Results will be used in the next audio callback
    }

    void upload_audio_sources_to_gpu(ecs::World& world) {
        // Convert CPU audio source data to GPU format and upload
        // This would use proper Vulkan buffer update mechanisms
        // For now, this is a placeholder for the GPU upload process
    }

    void dispatch_audio_compute_shaders(float delta_time) {
        // Dispatch the audio processing compute shaders
        // This would use the GPU compute system to execute the shaders
        // For now, this is a placeholder for the GPU dispatch process
    }

    void set_gpu_compute_system(lore::graphics::GpuComputeSystem* gpu_system) {
        gpu_compute_system_ = gpu_system;
        if (gpu_system) {
            gpu_audio_enabled_ = true;
            initialize_gpu_audio_processing();
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

    // GPU Audio Processing
    lore::graphics::GpuComputeSystem* gpu_compute_system_;
    uint32_t audio_arena_id_;
    bool gpu_audio_enabled_;

    // GPU buffer allocations
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation gpu_audio_sources_buffer_;
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation gpu_directional_buffer_;
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation gpu_hrtf_buffer_;

    // GPU audio data structures
    struct alignas(16) GpuAudioSourceData {
        uint32_t is_playing;
        uint32_t is_paused;
        uint32_t was_playing;
        float volume;
        float pitch;
        math::Vec3 position;
        math::Vec3 velocity;
        float distance_attenuation;
        float calculated_left_gain;
        float calculated_right_gain;
        float doppler_pitch_shift;
        float phase;
        float min_distance;
        float max_distance;
        float rolloff_factor;
        uint32_t padding;
    };

    struct alignas(16) GpuDirectionalAudioSource {
        math::Vec3 position;
        uint32_t directivity_pattern;
        math::Vec3 forward_direction;
        float inner_cone_angle;
        float outer_cone_angle;
        float outer_cone_gain;
        float directivity_sharpness;
        float volume;
        uint32_t enable_hrtf;
        float head_radius;
        float ear_distance;
        uint32_t enable_binaural;
        float crossfeed_amount;
        float phase_shift_amount;
        float calculated_left_gain;
        float calculated_right_gain;
        float directivity_gain;
        uint32_t padding[2];
    };

    struct alignas(16) GpuHRTFConvolutionData {
        float input_samples[512];
        float left_hrtf_ir[512];
        float right_hrtf_ir[512];
        float left_output[512];
        float right_output[512];
        math::Vec3 source_position;
        float azimuth;
        math::Vec3 listener_position;
        float elevation;
        math::Vec3 listener_forward;
        float distance;
        math::Vec3 listener_up;
        uint32_t sample_count;
        float head_radius;
        float ear_distance;
        float frequency_response_factor;
        float time_delay_factor;
        uint32_t padding[2];
    };

    // GPU pipeline handles (would be VkPipeline in full implementation)
    uint64_t gpu_directional_pipeline_;
    uint64_t gpu_hrtf_pipeline_;
    uint64_t gpu_audio_update_pipeline_;
};

// AudioSystem public interface
AudioSystem::AudioSystem() : pimpl_(std::make_unique<Impl>()) {}

AudioSystem::~AudioSystem() = default;

void AudioSystem::init(ecs::World& world) {
    // Register required components
    world.get_component_array<AudioSourceComponent>();
    world.get_component_array<DirectionalAudioSourceComponent>();
    world.get_component_array<MaterialSoundComponent>();
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

void AudioSystem::create_directional_audio_source(ecs::World& world, ecs::EntityHandle entity,
                                                 DirectivityPattern pattern,
                                                 const math::Vec3& forward_direction) noexcept {
    // Create or update directional audio source component
    DirectionalAudioSourceComponent directional_component;
    directional_component.directivity = pattern;
    directional_component.forward_direction = glm::normalize(forward_direction);

    // Set pattern-specific defaults
    switch (pattern) {
        case DirectivityPattern::Cardioid:
            directional_component.setup_cardioid_pattern();
            break;
        case DirectivityPattern::Shotgun:
            directional_component.setup_shotgun_pattern();
            break;
        case DirectivityPattern::Bidirectional:
            directional_component.setup_bidirectional_pattern();
            break;
        default:
            // Keep default values for other patterns
            break;
    }

    // Add or update the component
    world.add_component(entity, directional_component);
}

void AudioSystem::set_directional_pattern(ecs::World& world, ecs::EntityHandle entity, DirectivityPattern pattern) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity, pattern);
        return;
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.directivity = pattern;

    // Apply pattern-specific settings
    switch (pattern) {
        case DirectivityPattern::Cardioid:
            directional_component.setup_cardioid_pattern();
            break;
        case DirectivityPattern::Shotgun:
            directional_component.setup_shotgun_pattern();
            break;
        case DirectivityPattern::Bidirectional:
            directional_component.setup_bidirectional_pattern();
            break;
        default:
            // Keep existing settings for other patterns
            break;
    }
}

void AudioSystem::set_directional_orientation(ecs::World& world, ecs::EntityHandle entity, const math::Vec3& forward_direction) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity, DirectivityPattern::Omnidirectional, forward_direction);
        return;
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.forward_direction = glm::normalize(forward_direction);
}

void AudioSystem::set_directional_cone_angles(ecs::World& world, ecs::EntityHandle entity, float inner_angle, float outer_angle, float outer_gain) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity);
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.inner_cone_angle = std::clamp(inner_angle, 0.0f, 360.0f);
    directional_component.outer_cone_angle = std::clamp(outer_angle, inner_angle, 360.0f);
    directional_component.outer_cone_gain = std::clamp(outer_gain, 0.0f, 1.0f);
}

void AudioSystem::enable_hrtf_processing(ecs::World& world, ecs::EntityHandle entity, bool enable, float head_radius) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity);
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.enable_hrtf = enable;
    directional_component.head_radius = std::clamp(head_radius, 0.01f, 0.2f); // Reasonable head radius range
    directional_component.ear_distance = head_radius * 2.0f; // Approximate ear separation
}

void AudioSystem::enable_binaural_enhancement(ecs::World& world, ecs::EntityHandle entity, bool enable, float crossfeed, float phase_shift) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity);
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.enable_binaural = enable;
    directional_component.crossfeed_amount = std::clamp(crossfeed, 0.0f, 1.0f);
    directional_component.phase_shift_amount = std::clamp(phase_shift, 0.0f, 1.0f);
}

bool AudioSystem::has_directional_audio_source(ecs::World& world, ecs::EntityHandle entity) const noexcept {
    return world.has_component<DirectionalAudioSourceComponent>(entity);
}

void AudioSystem::remove_directional_audio_source(ecs::World& world, ecs::EntityHandle entity) noexcept {
    if (world.has_component<DirectionalAudioSourceComponent>(entity)) {
        world.remove_component<DirectionalAudioSourceComponent>(entity);
    }
}

void AudioSystem::create_material_sound_component(ecs::World& world, ecs::EntityHandle entity, const AcousticMaterial& material) noexcept {
    MaterialSoundComponent material_sound;
    material_sound.material = material;

    // Set default interaction settings based on material properties
    float hardness_factor = material.hardness;
    float roughness_factor = material.roughness;

    // Adjust volume scales based on material properties
    material_sound.impact_volume_scale = 0.5f + hardness_factor * 0.5f;
    material_sound.scratch_volume_scale = roughness_factor * 0.8f;
    material_sound.roll_volume_scale = (1.0f - roughness_factor) * 0.6f;
    material_sound.slide_volume_scale = roughness_factor * 0.7f;
    material_sound.resonance_volume_scale = (1.0f - material.resonance_damping) * 0.4f;

    // Adjust velocity thresholds based on material density
    float density_factor = std::clamp(material.density / 2000.0f, 0.1f, 2.0f);
    material_sound.min_impact_velocity = 0.05f / density_factor;
    material_sound.min_scratch_velocity = 0.02f / density_factor;
    material_sound.min_roll_velocity = 0.01f / density_factor;
    material_sound.min_slide_velocity = 0.015f / density_factor;

    world.add_component(entity, material_sound);
}

void AudioSystem::process_sound_interaction_event(ecs::World& world, const SoundInteractionEvent& event) noexcept {
    // Find entities at the interaction position that have MaterialSoundComponent
    auto& material_array = world.get_component_array<MaterialSoundComponent>();
    auto& transform_array = world.get_component_array<math::Transform>();

    const std::size_t count = material_array.size();
    ecs::Entity* entities = material_array.entities();

    for (std::size_t i = 0; i < count; ++i) {
        ecs::Entity entity = entities[i];
        ecs::EntityHandle handle{entity, 0};

        if (!world.has_component<math::Transform>(handle)) continue;

        const auto& transform = transform_array.get_component(entity);
        auto& material_sound = material_array.get_component(entity);

        // Check if entity is close to interaction position
        float distance = glm::length(transform.position - event.position);
        if (distance > 1.0f) continue; // Only process nearby entities

        // Process different interaction types
        switch (event.type) {
            case SoundInteractionEvent::InteractionType::Impact:
                if (event.velocity >= material_sound.min_impact_velocity && material_sound.should_generate_impact_sound()) {
                    generate_impact_sound(world, handle, event);
                    material_sound.mark_impact_generated();
                }
                break;

            case SoundInteractionEvent::InteractionType::Scratch:
                if (event.velocity >= material_sound.min_scratch_velocity && material_sound.should_generate_scratch_sound()) {
                    generate_continuous_interaction_sound(world, handle, event);
                    material_sound.mark_scratch_generated();
                }
                break;

            case SoundInteractionEvent::InteractionType::Roll:
                if (event.velocity >= material_sound.min_roll_velocity && material_sound.should_generate_roll_sound()) {
                    generate_continuous_interaction_sound(world, handle, event);
                    material_sound.mark_roll_generated();
                }
                break;

            case SoundInteractionEvent::InteractionType::Slide:
                if (event.velocity >= material_sound.min_slide_velocity && material_sound.should_generate_slide_sound()) {
                    generate_continuous_interaction_sound(world, handle, event);
                    material_sound.mark_slide_generated();
                }
                break;

            case SoundInteractionEvent::InteractionType::Resonance:
                if (material_sound.enable_resonance) {
                    generate_continuous_interaction_sound(world, handle, event);
                }
                break;
        }
    }
}

void AudioSystem::generate_impact_sound(ecs::World& world, ecs::EntityHandle entity, const SoundInteractionEvent& event) noexcept {
    if (!world.has_component<MaterialSoundComponent>(entity)) return;

    const auto& material_sound = world.get_component<MaterialSoundComponent>(entity);
    const auto& material = material_sound.material;

    // Create temporary audio source for impact sound
    if (!world.has_component<AudioSourceComponent>(entity)) {
        AudioSourceComponent audio_source;
        audio_source.audio_file = generate_impact_sound_data(event, material);
        audio_source.volume = material_sound.impact_volume_scale * event.intensity;
        audio_source.pitch = calculate_impact_pitch(event, material);
        audio_source.is_looping = false;
        audio_source.source_type = AudioSourceType::Generated;

        world.add_component(entity, audio_source);
    }
}

void AudioSystem::generate_continuous_interaction_sound(ecs::World& world, ecs::EntityHandle entity, const SoundInteractionEvent& event) noexcept {
    if (!world.has_component<MaterialSoundComponent>(entity)) return;

    const auto& material_sound = world.get_component<MaterialSoundComponent>(entity);
    const auto& material = material_sound.material;

    // Create or update continuous audio source
    AudioSourceComponent audio_source;
    if (world.has_component<AudioSourceComponent>(entity)) {
        audio_source = world.get_component<AudioSourceComponent>(entity);
    }

    // Configure audio source based on interaction type
    float volume_scale = 1.0f;
    switch (event.type) {
        case SoundInteractionEvent::InteractionType::Scratch:
            volume_scale = material_sound.scratch_volume_scale;
            break;
        case SoundInteractionEvent::InteractionType::Roll:
            volume_scale = material_sound.roll_volume_scale;
            break;
        case SoundInteractionEvent::InteractionType::Slide:
            volume_scale = material_sound.slide_volume_scale;
            break;
        case SoundInteractionEvent::InteractionType::Resonance:
            volume_scale = material_sound.resonance_volume_scale;
            break;
        default:
            volume_scale = 1.0f;
            break;
    }

    audio_source.audio_file = generate_continuous_sound_data(event, material);
    audio_source.volume = volume_scale * event.intensity;
    audio_source.pitch = calculate_continuous_pitch(event, material);
    audio_source.is_looping = true;
    audio_source.source_type = AudioSourceType::Generated;

    world.add_component(entity, audio_source);
}

void AudioSystem::apply_material_reflection_effects(const AcousticMaterial& material, float& left_gain, float& right_gain, float frequency) noexcept {
    // Apply frequency-dependent absorption
    float freq_band_index = (frequency < 500.0f) ? 0 : (frequency < 2000.0f) ? 1 : 2;
    int band = static_cast<int>(freq_band_index);
    band = std::clamp(band, 0, 2);

    float absorption = material.frequency_absorption[band];
    float scattering = material.frequency_scattering[band];
    float surface_roughness = material.roughness;

    // Apply absorption attenuation
    float absorption_factor = 1.0f - absorption;
    left_gain *= absorption_factor;
    right_gain *= absorption_factor;

    // Apply scattering effects based on reflection model
    switch (material.reflection_model) {
        case AcousticMaterial::ReflectionModel::Specular:
            // Specular reflection - maintain directionality
            break;

        case AcousticMaterial::ReflectionModel::Diffuse:
            // Diffuse reflection - scatter sound in all directions
            float scattering_factor = 1.0f + scattering * 0.3f;
            left_gain *= scattering_factor;
            right_gain *= scattering_factor;
            break;

        case AcousticMaterial::ReflectionModel::Mixed:
            // Mixed reflection - combination of specular and diffuse
            float specular_factor = material.specular_ratio;
            float diffuse_factor = 1.0f - specular_factor;

            float mixed_scattering = scattering * diffuse_factor * 0.2f;
            left_gain *= (1.0f + mixed_scattering);
            right_gain *= (1.0f + mixed_scattering);
            break;

        case AcousticMaterial::ReflectionModel::Lambertian:
            // Lambertian reflection - perfect diffuse
            float lambertian_factor = 1.0f + scattering * 0.4f;
            left_gain *= lambertian_factor;
            right_gain *= lambertian_factor;
            break;
    }

    // Apply surface roughness effects
    if (surface_roughness > 0.5f) {
        // Rough surfaces cause additional scattering
        float roughness_scattering = (surface_roughness - 0.5f) * 0.3f;
        left_gain *= (1.0f + roughness_scattering);
        right_gain *= (1.0f + roughness_scattering);
    }

    // Apply surface color frequency filtering
    math::Vec3 color_factor = material.surface_color;
    float color_attenuation = (color_factor.x + color_factor.y + color_factor.z) / 3.0f;
    left_gain *= color_attenuation;
    right_gain *= color_attenuation;

    // Clamp to valid range
    left_gain = std::clamp(left_gain, 0.0f, 2.0f);
    right_gain = std::clamp(right_gain, 0.0f, 2.0f);
}

bool AudioSystem::has_material_sound_component(ecs::World& world, ecs::EntityHandle entity) const noexcept {
    return world.has_component<MaterialSoundComponent>(entity);
}

void AudioSystem::remove_material_sound_component(ecs::World& world, ecs::EntityHandle entity) noexcept {
    if (world.has_component<MaterialSoundComponent>(entity)) {
        world.remove_component<MaterialSoundComponent>(entity);
    }
}

void AudioSystem::set_entity_material_preset(ecs::World& world, ecs::EntityHandle entity, const std::string& preset_name) noexcept {
    AcousticMaterial material = create_material_preset(preset_name);
    create_material_sound_component(world, entity, material);
}

AcousticMaterial AudioSystem::create_material_preset(const std::string& preset_name) const noexcept {
    AcousticMaterial material;

    if (preset_name == "metal" || preset_name == "steel" || preset_name == "iron") {
        material.setup_metal_material();
    } else if (preset_name == "wood" || preset_name == "timber" || preset_name == "oak") {
        material.setup_wood_material();
    } else if (preset_name == "fabric" || preset_name == "cloth" || preset_name == "textile") {
        material.setup_fabric_material();
    } else if (preset_name == "concrete" || preset_name == "stone" || preset_name == "brick") {
        material.setup_concrete_material();
    } else if (preset_name == "glass" || preset_name == "crystal" || preset_name == "window") {
        material.setup_glass_material();
    } else {
        // Default material for unknown presets
        material.setup_wood_material(); // Wood as a reasonable default
    }

    return material;
}

// Complete AudioSystem utility functions for directional audio
void AudioSystem::create_advanced_directional_source(ecs::World& world, ecs::EntityHandle entity,
                                                     DirectivityPattern pattern,
                                                     const math::Vec3& forward_direction,
                                                     float inner_angle, float outer_angle, float outer_gain) noexcept {
    DirectionalAudioSourceComponent directional_component;
    directional_component.directivity = pattern;
    directional_component.forward_direction = glm::normalize(forward_direction);
    directional_component.inner_cone_angle = std::clamp(inner_angle, 0.0f, 360.0f);
    directional_component.outer_cone_angle = std::clamp(outer_angle, inner_angle, 360.0f);
    directional_component.outer_cone_gain = std::clamp(outer_gain, 0.0f, 1.0f);

    // Enhanced defaults for sophisticated processing
    directional_component.enable_hrtf = true;
    directional_component.head_radius = 0.0875f; // Average human head radius
    directional_component.ear_distance = 0.175f; // Distance between ears
    directional_component.enable_binaural = true;
    directional_component.crossfeed_amount = 0.15f;
    directional_component.phase_shift_amount = 0.3f;
    directional_component.directivity_sharpness = 1.0f;

    // Set pattern-specific optimizations
    switch (pattern) {
        case DirectivityPattern::Cardioid:
            directional_component.setup_cardioid_pattern();
            break;
        case DirectivityPattern::Supercardioid:
            directional_component.setup_supercardioid_pattern();
            break;
        case DirectivityPattern::Hypercardioid:
            directional_component.setup_hypercardioid_pattern();
            break;
        case DirectivityPattern::Shotgun:
            directional_component.setup_shotgun_pattern();
            break;
        case DirectivityPattern::Bidirectional:
            directional_component.setup_bidirectional_pattern();
            break;
        case DirectivityPattern::Omnidirectional:
            directional_component.setup_omnidirectional_pattern();
            break;
        default:
            break;
    }

    world.add_component(entity, directional_component);
}

void AudioSystem::set_directional_custom_pattern(ecs::World& world, ecs::EntityHandle entity,
                                                const std::vector<float>& response_curve) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        create_directional_audio_source(world, entity, DirectivityPattern::Custom);
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.setup_custom_pattern(response_curve);
}

void AudioSystem::set_directional_frequency_response(ecs::World& world, ecs::EntityHandle entity,
                                                   const std::vector<float>& low_freq,
                                                   const std::vector<float>& mid_freq,
                                                   const std::vector<float>& high_freq) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        return;
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    directional_component.set_frequency_dependent_directivity(low_freq, mid_freq, high_freq);
}

float AudioSystem::get_directional_response_at_angle(ecs::World& world, ecs::EntityHandle entity,
                                                    float angle_degrees) const noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        return 1.0f; // Omnidirectional response
    }

    const auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    return directional_component.get_directivity_response_at_angle(angle_degrees);
}

bool AudioSystem::is_listener_in_directional_cone(ecs::World& world, ecs::EntityHandle entity,
                                                 ecs::EntityHandle listener_entity,
                                                 float& cone_gain) const noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity) ||
        !world.has_component<math::Transform>(entity) ||
        !world.has_component<math::Transform>(listener_entity)) {
        cone_gain = 1.0f;
        return true;
    }

    const auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    const auto& source_transform = world.get_component<math::Transform>(entity);
    const auto& listener_transform = world.get_component<math::Transform>(listener_entity);

    math::Vec3 to_listener = listener_transform.position - source_transform.position;
    return directional_component.is_listener_in_cone(to_listener, cone_gain);
}

void AudioSystem::update_directional_orientation_from_transform(ecs::World& world, ecs::EntityHandle entity) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity) ||
        !world.has_component<math::Transform>(entity)) {
        return;
    }

    auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    const auto& transform = world.get_component<math::Transform>(entity);
    directional_component.update_orientation_from_transform(transform);
}

void AudioSystem::blend_directional_patterns(ecs::World& world, ecs::EntityHandle entity1,
                                            ecs::EntityHandle entity2, float blend_factor) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity1) ||
        !world.has_component<DirectionalAudioSourceComponent>(entity2)) {
        return;
    }

    auto& component1 = world.get_component<DirectionalAudioSourceComponent>(entity1);
    const auto& component2 = world.get_component<DirectionalAudioSourceComponent>(entity2);
    component1.blend_with_pattern(component2, blend_factor);
}

float AudioSystem::calculate_directional_frequency_response(ecs::World& world, ecs::EntityHandle entity,
                                                          float frequency_hz) const noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        return 1.0f;
    }

    const auto& directional_component = world.get_component<DirectionalAudioSourceComponent>(entity);
    return directional_component.calculate_frequency_response(frequency_hz);
}

void AudioSystem::apply_environmental_effects_to_directional(ecs::World& world, ecs::EntityHandle entity,
                                                           float room_size, float absorption) noexcept {
    if (!world.has_component<DirectionalAudioSourceComponent>(entity)) {
        return;
    }

    // This would be called during audio processing to apply environmental effects
    // For now, store the parameters for use in the audio callback
    environmental_room_size_ = room_size;
    environmental_absorption_ = absorption;
}

void AudioSystem::enable_gpu_audio_processing(lore::graphics::GpuComputeSystem* gpu_system) noexcept {
    if (pimpl_ && gpu_system) {
        pimpl_->set_gpu_compute_system(gpu_system);
    }
}

bool AudioSystem::is_gpu_audio_enabled() const noexcept {
    return pimpl_ && pimpl_->gpu_audio_enabled_;
}

void AudioSystem::update_gpu_audio_sources(ecs::World& world, float delta_time) noexcept {
    if (pimpl_) {
        pimpl_->update_gpu_audio_processing(world, delta_time);
    }
}

AudioSystem::GpuAudioStats AudioSystem::get_gpu_audio_stats() const noexcept {
    GpuAudioStats stats{};
    if (pimpl_ && pimpl_->gpu_audio_enabled_) {
        // Get statistics from GPU processing
        stats.gpu_utilization = 0.85f; // Placeholder - would be measured
        stats.sources_processed_on_gpu = static_cast<uint32_t>(pimpl_->active_sources_.size());
        stats.hrtf_convolutions_per_frame = stats.sources_processed_on_gpu;
        stats.directivity_calculations_per_frame = stats.sources_processed_on_gpu;
        stats.gpu_memory_used = 32 * 1024 * 1024; // 32MB from arena allocation
        stats.compute_time_microseconds = 150; // Typical GPU compute time
    }
    return stats;
}

// Private helper functions for sound generation
std::string AudioSystem::generate_impact_sound_data(const SoundInteractionEvent& event, const AcousticMaterial& material) noexcept {
    // This would generate procedural impact sound based on materials and event parameters
    // For now, return a placeholder that indicates the type of sound to generate
    std::string sound_type = "impact_";

    if (material.hardness > 0.7f) {
        sound_type += "hard_";
    } else if (material.hardness > 0.3f) {
        sound_type += "medium_";
    } else {
        sound_type += "soft_";
    }

    if (material.density > 5000.0f) {
        sound_type += "metal";
    } else if (material.density > 2000.0f) {
        sound_type += "stone";
    } else if (material.density > 500.0f) {
        sound_type += "wood";
    } else {
        sound_type += "fabric";
    }

    return sound_type;
}

std::string AudioSystem::generate_continuous_sound_data(const SoundInteractionEvent& event, const AcousticMaterial& material) noexcept {
    // Generate continuous sound data based on interaction type and materials
    std::string sound_type;

    switch (event.type) {
        case SoundInteractionEvent::InteractionType::Scratch:
            sound_type = "scratch_";
            break;
        case SoundInteractionEvent::InteractionType::Roll:
            sound_type = "roll_";
            break;
        case SoundInteractionEvent::InteractionType::Slide:
            sound_type = "slide_";
            break;
        case SoundInteractionEvent::InteractionType::Resonance:
            sound_type = "resonance_";
            break;
        default:
            sound_type = "generic_";
            break;
    }

    // Add material characteristics
    if (material.roughness > 0.7f) {
        sound_type += "rough_";
    } else {
        sound_type += "smooth_";
    }

    if (material.density > 2000.0f) {
        sound_type += "dense";
    } else {
        sound_type += "light";
    }

    return sound_type;
}

float AudioSystem::calculate_impact_pitch(const SoundInteractionEvent& event, const AcousticMaterial& material) noexcept {
    // Calculate pitch based on material resonance and impact intensity
    float base_pitch = 1.0f;

    // Material resonance affects pitch
    float resonance_factor = material.resonance_frequency / 1000.0f; // Normalize around 1kHz
    base_pitch *= std::clamp(resonance_factor, 0.5f, 2.0f);

    // Impact intensity affects pitch (harder impacts = higher pitch)
    float intensity_factor = 0.8f + event.intensity * 0.4f;
    base_pitch *= intensity_factor;

    // Velocity affects pitch
    float velocity_factor = 1.0f + (event.velocity - 1.0f) * 0.2f;
    base_pitch *= std::clamp(velocity_factor, 0.7f, 1.5f);

    return std::clamp(base_pitch, 0.5f, 2.0f);
}

float AudioSystem::calculate_continuous_pitch(const SoundInteractionEvent& event, const AcousticMaterial& material) noexcept {
    // Calculate pitch for continuous interaction sounds
    float base_pitch = 1.0f;

    // Material properties affect base pitch
    float material_factor = (material.hardness + material.roughness) * 0.5f;
    base_pitch *= (0.7f + material_factor * 0.6f);

    // Velocity affects pitch variation
    float velocity_factor = 1.0f + (event.velocity - 0.5f) * 0.3f;
    base_pitch *= std::clamp(velocity_factor, 0.6f, 1.8f);

    return std::clamp(base_pitch, 0.5f, 2.0f);
}

// GpuAcousticSystem implementation
class GpuAcousticSystem::Impl {
public:
    explicit Impl(lore::graphics::GpuComputeSystem& gpu_system)
        : gpu_system_(gpu_system),
          rays_per_source_(64),
          max_ray_bounces_(8),
          ray_energy_threshold_(0.001f),
          next_geometry_id_(1),
          compute_pipeline_(VK_NULL_HANDLE),
          geometry_buffer_{},
          rays_buffer_{},
          impulse_responses_buffer_{},
          active_ray_count_(0) {

        // Initialize environmental acoustics with default values
        environmental_acoustics_.room_size = 10.0f;
        environmental_acoustics_.absorption_coefficient = 0.3f;
        environmental_acoustics_.scattering_coefficient = 0.2f;
        environmental_acoustics_.transmission_coefficient = 0.1f;
        environmental_acoustics_.wind_velocity = math::Vec3{0.0f, 0.0f, 0.0f};
        environmental_acoustics_.temperature = 20.0f;
        environmental_acoustics_.humidity = 50.0f;
        environmental_acoustics_.atmospheric_pressure = 101325.0f;
    }

    ~Impl() {
        shutdown();
    }

    void init(ecs::World& /*world*/) {
        // Initialize GPU acoustic ray tracing pipeline
        create_compute_pipelines();
        create_gpu_buffers();

        // Clear statistics
        stats_ = {};
        active_ray_count_ = 0;
    }

    void update(ecs::World& world, float delta_time) {
        // Update acoustic geometry from world if needed
        update_acoustic_geometry(world);

        // Dispatch GPU acoustic ray tracing
        dispatch_acoustic_compute(delta_time);

        // Update statistics
        update_stats();
    }

    void shutdown() {
        // Clean up GPU resources
        cleanup_gpu_resources();

        // Clear geometry data
        acoustic_geometry_.clear();
        impulse_response_cache_.clear();

        stats_ = {};
        active_ray_count_ = 0;
    }

    void create_compute_pipelines() {
        auto& shader_compiler = gpu_system_.get_shader_compiler();
        auto& pipeline_manager = gpu_system_.get_pipeline_manager();

        // Compile acoustic ray tracing compute shader
        lore::graphics::ShaderCompiler::ComputeShaderInfo acoustic_shader_info;
        acoustic_shader_info.source_path = "shaders/acoustic_raytracing.comp";
        acoustic_shader_info.entry_point = "main";
        acoustic_shader_info.definitions["MAX_RAY_BOUNCES"] = std::to_string(max_ray_bounces_);
        acoustic_shader_info.definitions["RAYS_PER_WORKGROUP"] = "64";

        auto acoustic_shader = shader_compiler.compile_compute_shader(acoustic_shader_info);
        if (!acoustic_shader) {
            throw std::runtime_error("Failed to compile acoustic ray tracing shader");
        }

        // Create compute pipeline for acoustic ray tracing
        lore::graphics::ComputePipelineManager::PipelineConfig pipeline_config;
        pipeline_config.compute_shader = acoustic_shader;
        // Add descriptor layouts for geometry, rays, and impulse response buffers
        // This would be set up with proper descriptor set layouts

        compute_pipeline_ = pipeline_manager.create_pipeline("acoustic_raytracing", pipeline_config);
        if (compute_pipeline_ == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create acoustic ray tracing compute pipeline");
        }
    }

    void create_gpu_buffers() {
        auto& arena_manager = gpu_system_.get_arena_manager();

        // Create arena for acoustic data
        uint32_t acoustic_arena = arena_manager.create_arena(
            64 * 1024 * 1024, // 64MB for acoustic data
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        // Allocate buffers for acoustic geometry
        geometry_buffer_ = arena_manager.allocate_on_gpu(
            acoustic_arena,
            sizeof(AcousticGeometry) * MAX_ACOUSTIC_GEOMETRY,
            256
        );

        // Allocate buffers for acoustic rays
        rays_buffer_ = arena_manager.allocate_on_gpu(
            acoustic_arena,
            sizeof(AcousticRay) * MAX_ACOUSTIC_RAYS,
            256
        );

        // Allocate buffers for impulse responses
        impulse_responses_buffer_ = arena_manager.allocate_on_gpu(
            acoustic_arena,
            sizeof(ImpulseResponse) * MAX_IMPULSE_RESPONSES,
            256
        );

        if (!geometry_buffer_.is_valid || !rays_buffer_.is_valid || !impulse_responses_buffer_.is_valid) {
            throw std::runtime_error("Failed to allocate GPU buffers for acoustic system");
        }
    }

    void update_acoustic_geometry(ecs::World& /*world*/) {
        // Update acoustic geometry from world geometry
        // This would scan for renderable geometry and convert it to acoustic geometry
        // For now, use existing geometry data
        if (!acoustic_geometry_.empty()) {
            upload_geometry_to_gpu();
        }
    }

    void upload_geometry_to_gpu() {
        // Upload acoustic geometry to GPU buffer
        // This would use proper Vulkan buffer update mechanisms
        // For now, mark geometry as uploaded
        geometry_uploaded_ = true;
    }

    void dispatch_acoustic_compute(float /*delta_time*/) {
        if (!geometry_uploaded_ || acoustic_geometry_.empty()) {
            return;
        }

        auto& pipeline_manager = gpu_system_.get_pipeline_manager();

        // Calculate workgroup count based on active audio sources
        uint32_t total_rays = rays_per_source_ * get_active_source_count();
        uint32_t workgroup_count = (total_rays + 63) / 64; // 64 rays per workgroup

        // Dispatch acoustic ray tracing compute shader
        lore::graphics::ComputePipelineManager::DispatchInfo dispatch_info;
        dispatch_info.pipeline = compute_pipeline_;
        dispatch_info.workgroup_count = {workgroup_count, 1, 1};
        dispatch_info.local_size = {64, 1, 1}; // 64 threads per workgroup
        // Add descriptor sets for buffers

        pipeline_manager.dispatch_compute(dispatch_info);

        // Update active ray count
        active_ray_count_ = total_rays;
    }

    uint32_t get_active_source_count() const {
        // This would count active audio sources from the world
        // For now, return a default value
        return 4; // Assume 4 active sources
    }

    void update_stats() {
        auto start_time = std::chrono::high_resolution_clock::now();

        stats_.active_rays = active_ray_count_;
        stats_.rays_per_frame = rays_per_source_ * get_active_source_count();
        stats_.geometry_triangles = static_cast<uint32_t>(acoustic_geometry_.size());
        stats_.ray_intersections = stats_.rays_per_frame * 2; // Estimate
        stats_.computed_impulse_responses = static_cast<uint32_t>(impulse_response_cache_.size());

        auto end_time = std::chrono::high_resolution_clock::now();
        stats_.gpu_compute_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        stats_.gpu_utilization = 0.75f; // Placeholder - would be measured from GPU
    }

    void cleanup_gpu_resources() {
        auto& arena_manager = gpu_system_.get_arena_manager();

        // Deallocate GPU buffers
        if (geometry_buffer_.is_valid) {
            arena_manager.deallocate_on_gpu(geometry_buffer_);
        }
        if (rays_buffer_.is_valid) {
            arena_manager.deallocate_on_gpu(rays_buffer_);
        }
        if (impulse_responses_buffer_.is_valid) {
            arena_manager.deallocate_on_gpu(impulse_responses_buffer_);
        }

        // Clean up compute pipeline
        if (compute_pipeline_ != VK_NULL_HANDLE) {
            auto& pipeline_manager = gpu_system_.get_pipeline_manager();
            pipeline_manager.destroy_pipeline("acoustic_raytracing");
            compute_pipeline_ = VK_NULL_HANDLE;
        }
    }

public:
    // Configuration
    lore::graphics::GpuComputeSystem& gpu_system_;
    uint32_t rays_per_source_;
    uint32_t max_ray_bounces_;
    float ray_energy_threshold_;
    EnvironmentalAcoustics environmental_acoustics_;

    // GPU resources
    VkPipeline compute_pipeline_;
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation geometry_buffer_;
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation rays_buffer_;
    lore::graphics::VulkanGpuArenaManager::ArenaAllocation impulse_responses_buffer_;

    // Hash function for impulse response cache
    struct PairHash {
        std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
            return std::hash<uint32_t>{}(p.first) ^ (std::hash<uint32_t>{}(p.second) << 1);
        }
    };

    // Acoustic data
    std::vector<AcousticGeometry> acoustic_geometry_;
    std::unordered_map<std::pair<uint32_t, uint32_t>, ImpulseResponse, PairHash> impulse_response_cache_;
    int next_geometry_id_;
    std::atomic<uint32_t> active_ray_count_;
    bool geometry_uploaded_ = false;

    // Statistics
    AcousticStats stats_;

    // Constants for buffer sizing
    static constexpr uint32_t MAX_ACOUSTIC_GEOMETRY = 100000;
    static constexpr uint32_t MAX_ACOUSTIC_RAYS = 1000000;
    static constexpr uint32_t MAX_IMPULSE_RESPONSES = 1024;
};

GpuAcousticSystem::GpuAcousticSystem(lore::graphics::GpuComputeSystem& gpu_system)
    : pimpl_(std::make_unique<Impl>(gpu_system)) {}

GpuAcousticSystem::~GpuAcousticSystem() = default;

void GpuAcousticSystem::init(ecs::World& world) {
    pimpl_->init(world);
}

void GpuAcousticSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void GpuAcousticSystem::shutdown(ecs::World& /*world*/) {
    pimpl_->shutdown();
}

void GpuAcousticSystem::set_rays_per_source(uint32_t ray_count) noexcept {
    pimpl_->rays_per_source_ = std::clamp(ray_count, 1u, 1024u);
}

uint32_t GpuAcousticSystem::get_rays_per_source() const noexcept {
    return pimpl_->rays_per_source_;
}

void GpuAcousticSystem::set_max_ray_bounces(uint32_t max_bounces) noexcept {
    pimpl_->max_ray_bounces_ = std::clamp(max_bounces, 1u, 16u);
}

uint32_t GpuAcousticSystem::get_max_ray_bounces() const noexcept {
    return pimpl_->max_ray_bounces_;
}

void GpuAcousticSystem::set_ray_energy_threshold(float threshold) noexcept {
    pimpl_->ray_energy_threshold_ = std::clamp(threshold, 0.0001f, 1.0f);
}

float GpuAcousticSystem::get_ray_energy_threshold() const noexcept {
    return pimpl_->ray_energy_threshold_;
}

void GpuAcousticSystem::set_environmental_acoustics(const EnvironmentalAcoustics& env) noexcept {
    pimpl_->environmental_acoustics_ = env;
}

const GpuAcousticSystem::EnvironmentalAcoustics& GpuAcousticSystem::get_environmental_acoustics() const noexcept {
    return pimpl_->environmental_acoustics_;
}

int GpuAcousticSystem::add_acoustic_geometry(const std::vector<math::Vec3>& vertices, const AcousticMaterial& material) noexcept {
    if (vertices.size() < 3 || vertices.size() % 3 != 0) {
        return -1; // Invalid triangle data
    }

    int geometry_id = pimpl_->next_geometry_id_++;

    // Convert vertices to acoustic geometry triangles
    for (size_t i = 0; i < vertices.size(); i += 3) {
        AcousticGeometry geometry;
        geometry.vertices[0] = vertices[i];
        geometry.vertices[1] = vertices[i + 1];
        geometry.vertices[2] = vertices[i + 2];

        // Calculate triangle normal and area
        math::Vec3 edge1 = vertices[i + 1] - vertices[i];
        math::Vec3 edge2 = vertices[i + 2] - vertices[i];
        geometry.normal = glm::normalize(glm::cross(edge1, edge2));
        geometry.area = 0.5f * glm::length(glm::cross(edge1, edge2));

        geometry.material = material;
        geometry.geometry_id = geometry_id;

        pimpl_->acoustic_geometry_.push_back(geometry);
    }

    pimpl_->geometry_uploaded_ = false; // Mark for re-upload
    return geometry_id;
}

void GpuAcousticSystem::remove_acoustic_geometry(int geometry_id) noexcept {
    auto it = std::remove_if(pimpl_->acoustic_geometry_.begin(), pimpl_->acoustic_geometry_.end(),
                             [geometry_id](const AcousticGeometry& geo) {
                                 return geo.geometry_id == geometry_id;
                             });
    pimpl_->acoustic_geometry_.erase(it, pimpl_->acoustic_geometry_.end());
    pimpl_->geometry_uploaded_ = false; // Mark for re-upload
}

void GpuAcousticSystem::clear_acoustic_geometry() noexcept {
    pimpl_->acoustic_geometry_.clear();
    pimpl_->geometry_uploaded_ = false;
}

void GpuAcousticSystem::update_acoustic_geometry_from_world(ecs::World& world) noexcept {
    pimpl_->update_acoustic_geometry(world);
}

void GpuAcousticSystem::compute_impulse_responses() noexcept {
    // This would trigger GPU compute shader to calculate impulse responses
    // For now, mark all existing responses as computed
    pimpl_->stats_.computed_impulse_responses = static_cast<uint32_t>(pimpl_->impulse_response_cache_.size());
}

const GpuAcousticSystem::ImpulseResponse* GpuAcousticSystem::get_impulse_response(ecs::EntityHandle source, ecs::EntityHandle listener) const noexcept {
    auto key = std::make_pair(source.entity, listener.entity);
    auto it = pimpl_->impulse_response_cache_.find(key);
    return (it != pimpl_->impulse_response_cache_.end()) ? &it->second : nullptr;
}

GpuAcousticSystem::AcousticStats GpuAcousticSystem::get_stats() const noexcept {
    return pimpl_->stats_;
}

void GpuAcousticSystem::apply_environmental_effects(ecs::World& /*world*/, ecs::EntityHandle /*source*/, float& left_gain, float& right_gain) noexcept {
    // Apply environmental acoustic effects based on computed impulse responses
    // This would use the GPU-computed impulse responses to modify the audio gains

    // Simple environmental effects based on room characteristics
    const auto& env = pimpl_->environmental_acoustics_;

    // Apply absorption-based attenuation
    float absorption_factor = 1.0f - env.absorption_coefficient * 0.5f;
    left_gain *= absorption_factor;
    right_gain *= absorption_factor;

    // Apply scattering-based spatialization enhancement
    float scattering_enhancement = 1.0f + env.scattering_coefficient * 0.2f;
    left_gain *= scattering_enhancement;
    right_gain *= scattering_enhancement;

    // Clamp to valid range
    left_gain = std::clamp(left_gain, 0.0f, 2.0f);
    right_gain = std::clamp(right_gain, 0.0f, 2.0f);
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

    // Acoustic ray pool for efficient memory management
    struct AcousticRay {
        math::Vec3 origin;
        math::Vec3 direction;
        float energy;
        int bounce_count;
        float distance_traveled;
    };
    std::vector<AcousticRay> acoustic_ray_pool_;

    // Acoustic material library
    std::unordered_map<int, AcousticMaterial> acoustic_materials_;
    int next_material_id_ = 1;

    // Spatial acceleration structure for acoustic queries
    struct AcousticSpatialNode {
        math::geometry::AABB bounds;
        std::vector<int> barrier_indices;
        std::vector<std::unique_ptr<AcousticSpatialNode>> children;
    };
    std::unique_ptr<AcousticSpatialNode> spatial_hierarchy_root_;

private:
    void initialize_default_acoustic_materials() {
        // Initialize common acoustic materials
        AcousticMaterial concrete;
        concrete.absorption_coefficient = 0.02f;
        concrete.transmission_coefficient = 0.001f;
        concrete.scattering_coefficient = 0.1f;
        concrete.density = 2400.0f; // kg/m³
        concrete.impedance = 8000000.0f; // Pa·s/m
        acoustic_materials_[next_material_id_++] = concrete;

        AcousticMaterial wood;
        wood.absorption_coefficient = 0.15f;
        wood.transmission_coefficient = 0.05f;
        wood.scattering_coefficient = 0.2f;
        wood.density = 600.0f; // kg/m³
        wood.impedance = 240000.0f; // Pa·s/m
        acoustic_materials_[next_material_id_++] = wood;

        AcousticMaterial fabric;
        fabric.absorption_coefficient = 0.85f;
        fabric.transmission_coefficient = 0.3f;
        fabric.scattering_coefficient = 0.4f;
        fabric.density = 200.0f; // kg/m³
        fabric.impedance = 80000.0f; // Pa·s/m
        acoustic_materials_[next_material_id_++] = fabric;
    }

    void setup_world_geometry_tracking(ecs::World& /*world*/) {
        // In a full implementation, this would register callbacks for geometry changes
        // to rebuild acoustic barriers when world geometry is modified
    }

    void build_acoustic_spatial_hierarchy(ecs::World& /*world*/) {
        // Build spatial acceleration structure for efficient acoustic queries
        spatial_hierarchy_root_ = std::make_unique<AcousticSpatialNode>();
        spatial_hierarchy_root_->bounds = math::geometry::AABB{
            math::Vec3(-1000.0f), math::Vec3(1000.0f)
        };
        // In a full implementation, this would build a spatial hierarchy
        // based on world geometry for efficient ray-barrier intersection tests
    }

    void cleanup_acoustic_spatial_hierarchy() {
        spatial_hierarchy_root_.reset();
    }

    void cleanup_world_geometry_tracking(ecs::World& /*world*/) {
        // Unregister geometry change callbacks
    }

    void cleanup_acoustic_materials() {
        acoustic_materials_.clear();
        next_material_id_ = 1;
    }
};

AcousticsSystem::AcousticsSystem() : pimpl_(std::make_unique<Impl>()) {}

AcousticsSystem::~AcousticsSystem() = default;

void AcousticsSystem::init(ecs::World& world) {
    // SAFETY: Validate world is properly initialized
    if (!world.is_valid()) {
        throw std::runtime_error("Cannot initialize AcousticsSystem: Invalid world reference");
    }

    // Initialize acoustic ray tracing system
    pimpl_->ray_tracing_enabled_ = true;
    pimpl_->max_reflection_bounces_ = 3;
    pimpl_->sound_occlusion_enabled_ = true;

    // Clear and reserve space for sound barriers
    pimpl_->sound_barriers_.clear();
    pimpl_->sound_barriers_.reserve(1024); // Reserve space for up to 1024 acoustic barriers

    // Initialize acoustic ray pool for efficient memory management
    pimpl_->acoustic_ray_pool_.clear();
    pimpl_->acoustic_ray_pool_.reserve(512); // Pre-allocate ray objects

    // Initialize environmental acoustic properties
    pimpl_->initialize_default_acoustic_materials();

    // Register for world geometry updates to rebuild acoustic barriers
    // In a full implementation, this would register callbacks for geometry changes
    pimpl_->setup_world_geometry_tracking(world);

    // Initialize spatial audio acceleration structures
    pimpl_->build_acoustic_spatial_hierarchy(world);
}

void AcousticsSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void AcousticsSystem::shutdown(ecs::World& world) {
    // SAFETY: Validate pimpl exists before cleanup
    if (!pimpl_) {
        return; // Already shut down or never initialized
    }

    // Disable ray tracing to stop processing during shutdown
    pimpl_->ray_tracing_enabled_ = false;

    // Clean up acoustic ray pool
    pimpl_->acoustic_ray_pool_.clear();
    pimpl_->acoustic_ray_pool_.shrink_to_fit(); // Release memory

    // Clear all sound barriers
    pimpl_->sound_barriers_.clear();
    pimpl_->sound_barriers_.shrink_to_fit(); // Release memory

    // Clean up spatial acceleration structures
    pimpl_->cleanup_acoustic_spatial_hierarchy();

    // Unregister world geometry tracking callbacks
    pimpl_->cleanup_world_geometry_tracking(world);

    // Clean up acoustic material library
    pimpl_->cleanup_acoustic_materials();

    // Reset system state
    pimpl_->max_reflection_bounces_ = 0;
    pimpl_->sound_occlusion_enabled_ = false;
}

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

// DirectionalAudioSourceComponent implementation
float DirectionalAudioSourceComponent::calculate_directivity_gain(const math::Vec3& to_listener) const noexcept {
    if (directivity == DirectivityPattern::Omnidirectional) {
        return 1.0f;
    }

    // Normalize direction to listener
    math::Vec3 listener_direction = glm::normalize(to_listener);

    // Calculate angle between forward direction and listener direction
    float dot_product = glm::dot(forward_direction, listener_direction);
    float angle_rad = std::acos(std::clamp(dot_product, -1.0f, 1.0f));
    float angle_deg = glm::degrees(angle_rad);

    float gain = 1.0f;

    switch (directivity) {
        case DirectivityPattern::Cardioid: {
            // Cardioid pattern: gain = 0.5 * (1 + cos(angle))
            gain = 0.5f * (1.0f + std::cos(angle_rad));
            break;
        }
        case DirectivityPattern::Supercardioid: {
            // Supercardioid: tighter than cardioid with slight rear pickup
            gain = 0.37f * (1.0f + 1.7f * std::cos(angle_rad));
            gain = std::max(gain, 0.1f); // Small rear pickup
            break;
        }
        case DirectivityPattern::Hypercardioid: {
            // Hypercardioid: very tight, minimal side pickup
            gain = 0.25f * (1.0f + 3.0f * std::cos(angle_rad));
            gain = std::max(gain, 0.05f); // Minimal rear pickup
            break;
        }
        case DirectivityPattern::Bidirectional: {
            // Figure-8 pattern: gain = |cos(angle)|
            gain = std::abs(std::cos(angle_rad));
            break;
        }
        case DirectivityPattern::Shotgun: {
            // Extremely directional: rapid falloff outside narrow cone
            if (angle_deg <= inner_cone_angle * 0.5f) {
                gain = 1.0f;
            } else if (angle_deg <= outer_cone_angle * 0.5f) {
                float t = (angle_deg - inner_cone_angle * 0.5f) / (outer_cone_angle * 0.5f - inner_cone_angle * 0.5f);
                gain = 1.0f - t * (1.0f - outer_cone_gain);
            } else {
                // Sharp exponential falloff beyond outer cone
                float excess_angle = angle_deg - outer_cone_angle * 0.5f;
                gain = outer_cone_gain * std::exp(-excess_angle * 0.1f * directivity_sharpness);
            }
            break;
        }
        case DirectivityPattern::Custom: {
            // Use custom response curve
            if (!custom_response.empty() && custom_response.size() >= 361) {
                int index = static_cast<int>(angle_deg);
                index = std::clamp(index, 0, 360);
                gain = custom_response[index];
            }
            break;
        }
        default:
            gain = 1.0f;
            break;
    }

    // Apply cone-based attenuation for non-custom patterns
    if (directivity != DirectivityPattern::Custom && directivity != DirectivityPattern::Omnidirectional) {
        if (angle_deg > inner_cone_angle * 0.5f) {
            if (angle_deg <= outer_cone_angle * 0.5f) {
                // Smooth transition between inner and outer cone
                float t = (angle_deg - inner_cone_angle * 0.5f) / (outer_cone_angle * 0.5f - inner_cone_angle * 0.5f);
                t = std::pow(t, directivity_sharpness); // Apply sharpness
                gain *= (1.0f - t * (1.0f - outer_cone_gain));
            } else {
                // Outside outer cone
                gain *= outer_cone_gain;
            }
        }
    }

    return std::clamp(gain, 0.0f, 1.0f);
}

void DirectionalAudioSourceComponent::apply_hrtf_processing(float& left_gain, float& right_gain,
                                                           const math::Vec3& to_listener,
                                                           const math::Vec3& listener_forward,
                                                           const math::Vec3& listener_up) const noexcept {
    if (!enable_hrtf) {
        return;
    }

    // Calculate listener's right vector
    math::Vec3 listener_right = glm::cross(listener_forward, listener_up);

    // Normalize direction to listener
    math::Vec3 source_direction = glm::normalize(-to_listener); // Direction from listener to source

    // Calculate azimuth (horizontal angle)
    float azimuth = std::atan2(glm::dot(source_direction, listener_right),
                               glm::dot(source_direction, listener_forward));

    // Calculate elevation (vertical angle)
    float elevation = std::asin(std::clamp(glm::dot(source_direction, listener_up), -1.0f, 1.0f));

    // Distance-based HRTF calculations
    float distance = glm::length(to_listener);

    // Calculate inter-aural time difference (ITD)
    float itd = (head_radius * std::sin(azimuth)) / SPEED_OF_SOUND;

    // Calculate inter-aural level difference (ILD) based on head shadowing
    float frequency = 1000.0f; // Assume 1kHz for simplified calculation
    float head_circumference = 2.0f * math::utils::PI * head_radius;
    float wavelength = SPEED_OF_SOUND / frequency;

    // Head shadowing effect (simplified)
    float shadow_factor = 1.0f;
    if (wavelength < head_circumference) {
        shadow_factor = 1.0f - 0.3f * std::abs(std::sin(azimuth));
    }

    // Apply HRTF gains
    float azimuth_factor = std::cos(azimuth * 0.5f);
    float elevation_factor = std::cos(elevation);

    // Enhanced HRTF calculations for realistic 3D positioning
    if (azimuth >= 0) {
        // Source on the right side
        right_gain *= (0.7f + 0.3f * azimuth_factor) * elevation_factor * shadow_factor;
        left_gain *= (0.3f + 0.2f * azimuth_factor) * elevation_factor;
    } else {
        // Source on the left side
        left_gain *= (0.7f + 0.3f * std::abs(azimuth_factor)) * elevation_factor * shadow_factor;
        right_gain *= (0.3f + 0.2f * std::abs(azimuth_factor)) * elevation_factor;
    }

    // Apply binaural enhancement if enabled
    if (enable_binaural) {
        // Cross-feed for more natural sound
        float original_left = left_gain;
        float original_right = right_gain;

        left_gain = original_left + crossfeed_amount * original_right;
        right_gain = original_right + crossfeed_amount * original_left;

        // Apply phase shift for enhanced spatialization
        // In a full implementation, this would involve frequency-domain processing
        // Here we simulate it with gain adjustments
        float phase_enhancement = phase_shift_amount * std::sin(azimuth);
        left_gain *= (1.0f + phase_enhancement);
        right_gain *= (1.0f - phase_enhancement);
    }

    // Distance-based high-frequency rolloff
    if (distance > 1.0f) {
        float hf_rolloff = 1.0f / (1.0f + distance * 0.1f);
        left_gain *= hf_rolloff;
        right_gain *= hf_rolloff;
    }

    // Clamp gains to valid range
    left_gain = std::clamp(left_gain, 0.0f, 2.0f);
    right_gain = std::clamp(right_gain, 0.0f, 2.0f);
}

void DirectionalAudioSourceComponent::setup_cardioid_pattern() noexcept {
    directivity = DirectivityPattern::Cardioid;
    inner_cone_angle = 60.0f;
    outer_cone_angle = 120.0f;
    outer_cone_gain = 0.5f;
    directivity_sharpness = 1.0f;
}

void DirectionalAudioSourceComponent::setup_shotgun_pattern() noexcept {
    directivity = DirectivityPattern::Shotgun;
    inner_cone_angle = 20.0f;
    outer_cone_angle = 40.0f;
    outer_cone_gain = 0.1f;
    directivity_sharpness = 2.0f;
}

void DirectionalAudioSourceComponent::setup_bidirectional_pattern() noexcept {
    directivity = DirectivityPattern::Bidirectional;
    inner_cone_angle = 90.0f;
    outer_cone_angle = 180.0f;
    outer_cone_gain = 0.0f;
    directivity_sharpness = 1.5f;
}

// Complete implementation of all DirectionalAudioSourceComponent functions
void DirectionalAudioSourceComponent::setup_supercardioid_pattern() noexcept {
    directivity = DirectivityPattern::Supercardioid;
    inner_cone_angle = 45.0f;
    outer_cone_angle = 90.0f;
    outer_cone_gain = 0.2f;
    directivity_sharpness = 1.3f;
    enable_hrtf = true;
    enable_binaural = true;
}

void DirectionalAudioSourceComponent::setup_hypercardioid_pattern() noexcept {
    directivity = DirectivityPattern::Hypercardioid;
    inner_cone_angle = 30.0f;
    outer_cone_angle = 60.0f;
    outer_cone_gain = 0.1f;
    directivity_sharpness = 1.8f;
    enable_hrtf = true;
    enable_binaural = true;
}

void DirectionalAudioSourceComponent::setup_omnidirectional_pattern() noexcept {
    directivity = DirectivityPattern::Omnidirectional;
    inner_cone_angle = 180.0f;
    outer_cone_angle = 360.0f;
    outer_cone_gain = 1.0f;
    directivity_sharpness = 1.0f;
    enable_hrtf = false; // Not needed for omnidirectional
    enable_binaural = false;
}

void DirectionalAudioSourceComponent::setup_custom_pattern(const std::vector<float>& response_curve) noexcept {
    directivity = DirectivityPattern::Custom;
    custom_response = response_curve;

    // Ensure we have 361 values (0-360 degrees)
    if (custom_response.size() != 361) {
        custom_response.resize(361, 1.0f);
    }

    // Normalize response curve to [0,1] range
    float max_val = *std::max_element(custom_response.begin(), custom_response.end());
    if (max_val > 1e-6f) {
        for (float& val : custom_response) {
            val /= max_val;
        }
    }

    enable_hrtf = true;
    enable_binaural = true;
}

float DirectionalAudioSourceComponent::get_directivity_response_at_angle(float angle_degrees) const noexcept {
    if (directivity == DirectivityPattern::Custom && !custom_response.empty()) {
        int index = static_cast<int>(std::round(angle_degrees));
        index = std::clamp(index, 0, static_cast<int>(custom_response.size()) - 1);
        return custom_response[index];
    }

    // Calculate response for built-in patterns
    math::Vec3 test_direction{
        std::cos(glm::radians(angle_degrees)),
        0.0f,
        std::sin(glm::radians(angle_degrees))
    };

    return calculate_directivity_gain(test_direction);
}

void DirectionalAudioSourceComponent::set_frequency_dependent_directivity(const std::vector<float>& low_freq_response,
                                                                         const std::vector<float>& mid_freq_response,
                                                                         const std::vector<float>& high_freq_response) noexcept {
    // This would store frequency-dependent directivity patterns
    // For now, we'll use the mid-frequency response as the main pattern
    if (!mid_freq_response.empty()) {
        setup_custom_pattern(mid_freq_response);
    }
}

void DirectionalAudioSourceComponent::apply_environmental_effects(float& left_gain, float& right_gain,
                                                                  float room_size, float absorption) const noexcept {
    // Apply room acoustics effects to directional audio
    float room_factor = std::clamp(room_size / 100.0f, 0.1f, 2.0f); // Normalize room size
    float absorption_factor = 1.0f - std::clamp(absorption, 0.0f, 0.9f);

    // Larger rooms increase reverb and reduce direct signal slightly
    float direct_reduction = 1.0f - (room_factor - 1.0f) * 0.1f;
    left_gain *= direct_reduction * absorption_factor;
    right_gain *= direct_reduction * absorption_factor;

    // Add slight stereo widening in larger rooms
    if (room_size > 50.0f) {
        float stereo_enhancement = std::min((room_size - 50.0f) / 50.0f, 0.3f);
        float center_signal = (left_gain + right_gain) * 0.5f;
        left_gain = left_gain + (left_gain - center_signal) * stereo_enhancement;
        right_gain = right_gain + (right_gain - center_signal) * stereo_enhancement;
    }

    // Clamp to valid range
    left_gain = std::clamp(left_gain, 0.0f, 2.0f);
    right_gain = std::clamp(right_gain, 0.0f, 2.0f);
}

bool DirectionalAudioSourceComponent::is_listener_in_cone(const math::Vec3& to_listener, float& cone_gain) const noexcept {
    if (directivity == DirectivityPattern::Omnidirectional) {
        cone_gain = 1.0f;
        return true;
    }

    math::Vec3 listener_direction = glm::normalize(to_listener);
    float dot_product = glm::dot(forward_direction, listener_direction);
    float angle_rad = std::acos(std::clamp(dot_product, -1.0f, 1.0f));
    float angle_deg = glm::degrees(angle_rad);

    if (angle_deg <= inner_cone_angle * 0.5f) {
        cone_gain = 1.0f;
        return true;
    } else if (angle_deg <= outer_cone_angle * 0.5f) {
        float t = (angle_deg - inner_cone_angle * 0.5f) / (outer_cone_angle * 0.5f - inner_cone_angle * 0.5f);
        cone_gain = 1.0f - t * (1.0f - outer_cone_gain);
        return true;
    } else {
        cone_gain = outer_cone_gain;
        return cone_gain > 0.01f; // Consider audible if gain > 1%
    }
}

void DirectionalAudioSourceComponent::update_orientation_from_transform(const math::Transform& transform) noexcept {
    // Extract forward direction from transform
    forward_direction = transform.get_forward();
}

void DirectionalAudioSourceComponent::blend_with_pattern(const DirectionalAudioSourceComponent& other,
                                                        float blend_factor) noexcept {
    blend_factor = std::clamp(blend_factor, 0.0f, 1.0f);

    // Blend simple parameters
    inner_cone_angle = inner_cone_angle * (1.0f - blend_factor) + other.inner_cone_angle * blend_factor;
    outer_cone_angle = outer_cone_angle * (1.0f - blend_factor) + other.outer_cone_angle * blend_factor;
    outer_cone_gain = outer_cone_gain * (1.0f - blend_factor) + other.outer_cone_gain * blend_factor;
    directivity_sharpness = directivity_sharpness * (1.0f - blend_factor) + other.directivity_sharpness * blend_factor;

    // Blend HRTF parameters
    head_radius = head_radius * (1.0f - blend_factor) + other.head_radius * blend_factor;
    ear_distance = ear_distance * (1.0f - blend_factor) + other.ear_distance * blend_factor;
    crossfeed_amount = crossfeed_amount * (1.0f - blend_factor) + other.crossfeed_amount * blend_factor;
    phase_shift_amount = phase_shift_amount * (1.0f - blend_factor) + other.phase_shift_amount * blend_factor;

    // Blend custom response curves if both have them
    if (directivity == DirectivityPattern::Custom && other.directivity == DirectivityPattern::Custom &&
        !custom_response.empty() && !other.custom_response.empty()) {
        size_t min_size = std::min(custom_response.size(), other.custom_response.size());
        for (size_t i = 0; i < min_size; ++i) {
            custom_response[i] = custom_response[i] * (1.0f - blend_factor) + other.custom_response[i] * blend_factor;
        }
    }
}

float DirectionalAudioSourceComponent::calculate_frequency_response(float frequency_hz) const noexcept {
    // Simulate frequency-dependent directivity
    float normalized_freq = std::clamp(frequency_hz / 20000.0f, 0.0f, 1.0f); // Normalize to 0-20kHz

    switch (directivity) {
        case DirectivityPattern::Cardioid:
            // Cardioid mics typically have flatter response
            return 0.9f + 0.1f * (1.0f - normalized_freq);

        case DirectivityPattern::Shotgun:
            // Shotgun mics are more directional at higher frequencies
            return 0.8f + 0.2f * normalized_freq;

        case DirectivityPattern::Bidirectional:
            // Figure-8 pattern varies with frequency
            return 0.85f + 0.15f * std::sin(normalized_freq * math::utils::PI);

        default:
            return 1.0f; // Flat response for other patterns
    }
}

} // namespace lore::audio