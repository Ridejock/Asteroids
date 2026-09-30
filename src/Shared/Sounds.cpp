#include "Sounds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <Emerald/Audio/Synth.h>

namespace Asteroids {

namespace {

using namespace Emerald::Synth;

// Noise that gets darker (lower "pitch") as it fades: the base of every explosion.
Emerald::Sound Explosion(f32 seconds, f32 startHz, f32 endHz, f32 cutoffHz, f32 halfLife,
                         f32 volume, u32 seed)
{
    std::vector<f32> s = Generate({.Shape = Wave::Noise,
                                   .Seconds = seconds,
                                   .StartHz = startHz,
                                   .EndHz = endHz,
                                   .Volume = volume},
                                  seed);
    LowPass(s, cutoffHz);
    ApplyDecay(s, halfLife);
    return ToSound(s);
}

// Makes the end flow into the start: the last `fade` seconds are cross-faded into the beginning
// and then cut off, so the sound can loop without a click or a gap.
std::vector<f32> MakeLoopable(std::vector<f32> s, f32 fade)
{
    const usize n = static_cast<usize>(fade * static_cast<f32>(Emerald::kMixSpec.freq));
    const usize length = s.size() - n;
    for (usize i = 0; i < n; ++i) {
        const f32 t = static_cast<f32>(i) / static_cast<f32>(n); // 0 -> 1 across the fade
        s[i] = s[i] * t + s[length + i] * (1.0f - t);
    }
    s.resize(length);
    return s;
}

// One low "thump": a lowpassed square wave with a fast decay.
Emerald::Sound Thump(f32 hz)
{
    std::vector<f32> s =
        Generate({.Shape = Wave::Square, .Seconds = 0.12f, .StartHz = hz, .Volume = 0.5f});
    LowPass(s, 500.0f);
    ApplyAdsr(s, {.Attack = 0.002f, .Decay = 0.05f, .Sustain = 0.6f, .Release = 0.04f});
    return ToSound(s);
}

// The saucer's siren: a square wave whose pitch wobbles up and down, looped. The loop is a whole
// number of wobbles long, so the cross-fade joins matching parts of the wobble.
Emerald::Sound Warble(f32 hz, f32 wobbleHz, f32 volume)
{
    constexpr f32 kLoopSeconds = 1.0f; // wobbleHz * this should be a whole number
    constexpr f32 kFade = 0.05f;
    std::vector<f32> s = Generate({.Shape = Wave::Square,
                                   .Seconds = kLoopSeconds + kFade,
                                   .StartHz = hz,
                                   .Volume = volume,
                                   .VibratoHz = wobbleHz,
                                   .VibratoDepth = 0.2f});
    LowPass(s, 2500.0f);
    return ToSound(MakeLoopable(std::move(s), kFade));
}

f32 Peak(const Emerald::Sound& sound)
{
    f32 peak = 0.0f;
    for (f32 s : sound.GetSamples())
        peak = std::max(peak, std::abs(s));
    return peak;
}

// A copy of `sound` scaled so its loudest sample is at `target`.
Emerald::Sound WithPeak(const Emerald::Sound& sound, f32 target)
{
    const f32 peak = Peak(sound);
    if (peak < 1e-6f)
        return sound; // silent file: nothing to scale
    std::vector<f32> samples(sound.GetSamples().begin(), sound.GetSamples().end());
    for (f32& s : samples)
        s *= target / peak;
    return Emerald::Sound(std::move(samples));
}

// Music and ambience have no generated version to match; their play volume is set in Main.cpp.
constexpr f32 kBackgroundPeak = 0.8f;

} // namespace

Sounds MakeSounds()
{
    Sounds sounds;

    // Fire: a square wave sweeping down quickly, softened a little.
    std::vector<f32> fire = Generate({.Shape = Wave::Square,
                                      .Seconds = 0.12f,
                                      .StartHz = 1400.0f,
                                      .EndHz = 350.0f,
                                      .Volume = 0.22f});
    LowPass(fire, 6000.0f);
    ApplyDecay(fire, 0.04f);
    sounds.Fire = ToSound(fire);

    // Thrust: dark rumbling noise, one second, looped.
    std::vector<f32> thrust =
        Generate({.Shape = Wave::Noise, .Seconds = 1.25f, .StartHz = 2500.0f, .Volume = 0.9f}, 7);
    LowPass(thrust, 450.0f);
    LowPass(thrust, 900.0f); // a second pass makes it rounder
    sounds.Thrust = ToSound(MakeLoopable(std::move(thrust), 0.25f));

    sounds.ExplosionLarge = Explosion(1.0f, 2200.0f, 250.0f, 900.0f, 0.18f, 0.65f, 11);
    sounds.ExplosionMedium = Explosion(0.7f, 3500.0f, 450.0f, 1500.0f, 0.12f, 0.55f, 12);
    sounds.ExplosionSmall = Explosion(0.45f, 5000.0f, 800.0f, 2600.0f, 0.07f, 0.45f, 13);

    // Ship: a long crash plus a deep falling rumble underneath.
    std::vector<f32> crash = Generate({.Shape = Wave::Noise,
                                       .Seconds = 1.6f,
                                       .StartHz = 2000.0f,
                                       .EndHz = 150.0f,
                                       .Volume = 0.7f},
                                      21);
    LowPass(crash, 800.0f);
    std::vector<f32> rumble = Generate(
        {.Shape = Wave::Sine, .Seconds = 1.6f, .StartHz = 90.0f, .EndHz = 35.0f, .Volume = 0.4f});
    MixInto(crash, rumble);
    ApplyDecay(crash, 0.35f);
    sounds.ShipExplosion = ToSound(crash);

    // Extra life: four quick high beeps.
    std::vector<f32> extra = Silence(0.48f);
    std::vector<f32> beep =
        Generate({.Shape = Wave::Square, .Seconds = 0.06f, .StartHz = 1500.0f, .Volume = 0.18f});
    ApplyAdsr(beep, {.Attack = 0.002f, .Decay = 0.0f, .Sustain = 1.0f, .Release = 0.01f});
    for (usize i = 0; i < 4; ++i)
        MixInto(std::span<f32>(extra).subspan(i * extra.size() / 4), beep);
    sounds.ExtraLife = ToSound(extra);

    // Hyperspace: noise rising in pitch, fading in and out.
    std::vector<f32> whoosh = Generate({.Shape = Wave::Noise,
                                        .Seconds = 0.45f,
                                        .StartHz = 800.0f,
                                        .EndHz = 7000.0f,
                                        .Volume = 0.4f},
                                       31);
    LowPass(whoosh, 3000.0f);
    ApplyAdsr(whoosh, {.Attack = 0.12f, .Decay = 0.1f, .Sustain = 0.8f, .Release = 0.2f});
    sounds.Hyperspace = ToSound(whoosh);

    // The heartbeat: two low tones a few semitones apart.
    sounds.BeatHigh = Thump(62.0f);
    sounds.BeatLow = Thump(52.0f);

    // Saucers: the big one lower and slower, the small one higher and faster (and more urgent).
    sounds.SaucerLarge = Warble(330.0f, 4.0f, 0.16f);
    sounds.SaucerSmall = Warble(760.0f, 8.0f, 0.13f);

    // Saucer shot: a thin pulse wave sweeping down, a little lower than the ship's.
    std::vector<f32> saucerFire = Generate({.Shape = Wave::Square,
                                            .Seconds = 0.14f,
                                            .StartHz = 1000.0f,
                                            .EndHz = 280.0f,
                                            .Duty = 0.2f,
                                            .Volume = 0.2f});
    LowPass(saucerFire, 5000.0f);
    ApplyDecay(saucerFire, 0.045f);
    sounds.SaucerFire = ToSound(saucerFire);
    return sounds;
}

std::vector<std::string> LoadOverrides(Sounds& sounds, const std::filesystem::path& folder)
{
    struct Override {
        const char* Name;
        Emerald::Sound Sounds::* Member;
    };
    static constexpr std::array<Override, 15> kOverrides{{
        {"fire", &Sounds::Fire},
        {"thrust", &Sounds::Thrust},
        {"bang_large", &Sounds::ExplosionLarge},
        {"bang_medium", &Sounds::ExplosionMedium},
        {"bang_small", &Sounds::ExplosionSmall},
        {"ship_explode", &Sounds::ShipExplosion},
        {"extra_life", &Sounds::ExtraLife},
        {"beat1", &Sounds::BeatHigh},
        {"beat2", &Sounds::BeatLow},
        {"hyperspace", &Sounds::Hyperspace},
        {"saucer_large", &Sounds::SaucerLarge},
        {"saucer_small", &Sounds::SaucerSmall},
        {"saucer_fire", &Sounds::SaucerFire},
        {"music", &Sounds::Music},
        {"ambience", &Sounds::Ambience},
    }};

    std::vector<std::string> loaded;
    for (const Override& o : kOverrides) {
        for (const char* extension : {".mp3", ".wav"}) {
            const std::filesystem::path file = folder / (std::string(o.Name) + extension);
            std::error_code error; // missing folder or file: just not overridden
            if (!std::filesystem::is_regular_file(file, error))
                continue;
            // LoadSound logs why a file could not be used; the generated sound stays then.
            if (std::optional<Emerald::Sound> sound = Emerald::LoadSound(file)) {
                // Scaled to the level of the generated sound it replaces, so a full-scale clip
                // does not drown out everything else (or keep the limiter busy).
                Emerald::Sound& slot = sounds.*o.Member;
                slot = WithPeak(*sound, slot.IsEmpty() ? kBackgroundPeak : Peak(slot));
                loaded.emplace_back(o.Name);
            }
            break; // .mp3 wins over .wav
        }
    }
    return loaded;
}

} // namespace Asteroids
