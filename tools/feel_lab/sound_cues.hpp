#pragma once

#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace feel_lab {

class SoundCues {
public:
    enum Cue { Pistol, Rifle, Step, Click, Detonation, Count };

    SoundCues()=default;
    SoundCues(const SoundCues&)=delete;
    SoundCues& operator=(const SoundCues&)=delete;

    void load() {
        InitAudioDevice();
        if (!IsAudioDeviceReady()) return;
        ready=true;
        constexpr unsigned sample_rate=24000;
        constexpr std::array<float,Count> durations{0.13f,0.22f,0.09f,0.08f,0.55f};
        constexpr std::array<float,Count> tones{140,76,82,890,42};
        constexpr std::array<float,Count> noise_mix{0.72f,0.66f,0.48f,0.09f,0.58f};
        std::uint32_t seed=10753;
        for (std::size_t index=0; index<sounds.size(); ++index) {
            const auto count=static_cast<unsigned>(durations[index]*sample_rate);
            std::vector<std::int16_t> samples(count);
            float filtered=0;
            for (unsigned i=0;i<count;++i) {
                const float seconds=static_cast<float>(i)/sample_rate;
                seed=seed*1664525u+1013904223u;
                const float white=static_cast<float>(seed>>16)/32768.0f-1.0f;
                filtered+=(white-filtered)*(index==Step || index==Detonation?0.18f:0.70f);
                const float tone=std::sin(6.2831853f*tones[index]*seconds);
                const float envelope=std::exp(-6*seconds/durations[index])*
                                     std::min(1.0f,static_cast<float>(i)/16);
                const float value=(filtered*noise_mix[index]+tone*(1-noise_mix[index]))*envelope;
                samples[i]=static_cast<std::int16_t>(value*27000);
            }
            sounds[index]=LoadSoundFromWave({count,sample_rate,16,1,samples.data()});
        }
    }

    void play(Cue cue,float volume,float pan=0.5f) const {
        if (!ready || !IsSoundValid(sounds[cue]) || volume<=0) return;
        SetSoundVolume(sounds[cue],volume);
        SetSoundPan(sounds[cue],pan);
        PlaySound(sounds[cue]);
    }

    ~SoundCues() {
        if (!ready) return;
        for (const auto sound : sounds)
            if (IsSoundValid(sound)) UnloadSound(sound);
        CloseAudioDevice();
    }

private:
    std::array<Sound,Count> sounds{};
    bool ready=false;
};

} // namespace feel_lab
