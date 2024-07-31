#include <iostream>
#include <filesystem>
#include <array>
#include <atomic>
#include <thread>
#include <shared_mutex>

#include <rtaudio/RtAudio.h>
#include "audio.hpp"
#include "AudioFile.h"

#include <utilities/debug.hpp>

struct Sound {
    std::filesystem::path path;
    AudioFile<double> audioFile;

    [[nodiscard]] auto sample(float time) const -> float;
};

auto sounds = []() {
    auto result = std::array{
        Sound{.path = "assets/footstep0.wav"},
        Sound{.path = "assets/footstep1.wav"},
        Sound{.path = "assets/footstep2.wav"},
        Sound{.path = "assets/landing0.wav"},
        Sound{.path = "assets/landing1.wav"},
        Sound{.path = "assets/jump0.wav"},
    };

    return result;
}();

static const float PI = 3.1415926535f;
float const float_sample_rate = 44100.0f;

struct SoundInstance {
    int index;
    int seed;
    float duration;
    float playback_head = 0.0f;
    [[nodiscard]] auto sample(float time) const -> float;
};

auto is_running = std::atomic_bool{};

auto sounds_mutex = std::shared_mutex{};
auto sound_instances = std::vector<SoundInstance>{};

auto audio_thread = std::thread{};

constexpr auto smoothstep(float edge0, float edge1, float x) -> float {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

constexpr auto sign(float x) -> float {
    return x < 0.0f ? -1.0f : 1.0f;
}

auto sine_wave(float t) -> float {
    return sinf(t * 2.0f * PI);
}

auto triangle_wave(float t) -> float {
    return abs(2 - abs(4 * (t - floor(t))) - 1) - 1;
}

auto square_wave(float t) -> float {
    return smoothstep(-0.5f, 0.5f, triangle_wave(t)) * 2.0f - 1.0f;
}

[[nodiscard]] auto Sound::sample(float time) const -> float {
    int channel = 0;
    int sample_index = int(time * float(audioFile.getSampleRate()));
    if (sample_index >= audioFile.getNumSamplesPerChannel()) {
        return 0;
    }
    return audioFile.samples[channel][sample_index];
}

[[nodiscard]] auto SoundInstance::sample(float time) const -> float {
    float t = time / duration;
    float volume = smoothstep(0.0f, 0.1f, t) * smoothstep(1.0f, 0.4f, t);

    if (index == 0) {
        return sounds[0 + seed % 3].sample(time) * volume;
        // return sine_wave(time * 440.0f) * volume * 0.05f;
    }
    if (index == 1) {
        return sounds[5].sample(time) * volume;
        // return sine_wave(time * 554.37f) * volume * 0.05f;
    }
    if (index == 2) {
        return sine_wave(time * 659.25f) * volume * 0.05f;
    }
    if (index == 3) {
        return sine_wave(time * 880.0f) * volume * 0.05f;
    }
    if (index == 4) {
        return sine_wave(time * 220.25f) * volume * 0.05f;
    }
    if (index == 5) {
        return sine_wave(time * 277.185f) * volume * 0.05f;
    }
    if (index == 6) {
        float result = sounds[3 + 0].sample(time) * volume;
        if (seed % 50 < 30) {
            result += sounds[3 + 1].sample(time) * volume;
        }
        return result;

        // return triangle_wave(time * 329.625f) * volume * 0.05f;
    }
    return 0;
}

auto write_callback(void *output_buffer, void *inputBuffer, unsigned int frames_left,
                    double streamTime, RtAudioStreamStatus status, void *userData) -> int {
    if (status) {
        debug_utils::add_log(g_console, "Stream underflow detected!");
    }
    float seconds_per_frame = 1.0f / float_sample_rate;
    int err = -1;
    while (frames_left > 0) {
        int frame_count = frames_left;
        if (is_running.load() != true) {
            break;
        }
        for (int frame = 0; frame < frame_count; frame += 1) {
            float sample = 0.0f;
            {
                auto lock = std::shared_lock{sounds_mutex};
                for (auto const &sound_instance : sound_instances) {
                    auto offset = sound_instance.playback_head + frame * seconds_per_frame;
                    if (offset < sound_instance.duration) {
                        sample += sound_instance.sample(offset);
                    }
                }
            }
            for (int channel = 0; channel < 2; channel += 1) {
                double *ptr = static_cast<double *>(output_buffer) + (frame * 2 + channel);
                *ptr = sample;
            }
        }
        frames_left -= frame_count;
        for (auto &sound_instance : sound_instances) {
            sound_instance.playback_head += frame_count * seconds_per_frame;
        }
    }
    {
        auto lock = std::unique_lock{sounds_mutex};
        std::erase_if(sound_instances, [&](auto sound_instance) -> bool { return sound_instance.playback_head >= sound_instance.duration; });
    }
    return 0;
}

void audio_thread_main() {
    RtAudio dac;
    if (dac.getDeviceCount() < 1) {
        debug_utils::add_log(g_console, "No audio devices found!");
        return;
    }
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;
    unsigned int bufferFrames = 256;
    dac.openStream(&parameters, nullptr, RTAUDIO_FLOAT64, float_sample_rate, &bufferFrames, &write_callback, nullptr);
    // Stream is open ... now start it.
    dac.startStream();

    while (is_running.load()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }

    if (dac.isStreamRunning()) {
        dac.stopStream(); // or could call dac.abortStream();
    }
    if (dac.isStreamOpen()) {
        dac.closeStream();
    }
}

void audio::init() {
    is_running.store(true);

    for (auto &sound : sounds) {
        sound.audioFile.load(sound.path.string());
    }

    audio_thread = std::thread(audio_thread_main);
}

void audio::deinit() {
    is_running.store(false);
    audio_thread.join();
}

void audio::play_sound(int sound_index) {
    auto lock = std::unique_lock{sounds_mutex};
    sound_instances.push_back({.index = sound_index % 7, .seed = rand(), .duration = 0.4f});
}
