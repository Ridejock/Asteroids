// Tests for LoadOverrides with generated files in a temporary folder (no audio is committed and
// no audio device is needed).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"
#include "Sounds.h"

namespace {

// Writes `seconds` of a 440 Hz sine as a mono 16-bit 8 kHz WAV file.
void WriteWav(const std::filesystem::path& path, f32 seconds)
{
    constexpr u32 kRate = 8000;
    std::vector<i16> samples(static_cast<usize>(seconds * static_cast<f32>(kRate)));
    for (usize i = 0; i < samples.size(); ++i)
        samples[i] = static_cast<i16>(
            8000.0 * std::sin(2.0 * 3.14159265358979 * 440.0 * static_cast<f64>(i) / kRate));
    const auto put32 = [](std::ofstream& f, u32 v) { f.write(reinterpret_cast<char*>(&v), 4); };
    const auto put16 = [](std::ofstream& f, u16 v) { f.write(reinterpret_cast<char*>(&v), 2); };
    const u32 bytes = static_cast<u32>(samples.size() * sizeof(i16));
    std::ofstream f(path, std::ios::binary);
    f.write("RIFF", 4);
    put32(f, 36 + bytes);
    f.write("WAVEfmt ", 8);
    put32(f, 16);
    put16(f, 1); // PCM
    put16(f, 1); // mono
    put32(f, kRate);
    put32(f, kRate * 2);
    put16(f, 2);
    put16(f, 16);
    f.write("data", 4);
    put32(f, bytes);
    f.write(reinterpret_cast<const char*>(samples.data()), bytes);
}

f32 Peak(const Emerald::Sound& sound)
{
    f32 peak = 0.0f;
    for (f32 s : sound.GetSamples())
        peak = std::max(peak, std::abs(s));
    return peak;
}

bool Near(f32 a, f32 b)
{
    return std::abs(a - b) < 0.01f;
}

} // namespace

int main()
{
    const std::filesystem::path folder =
        std::filesystem::temp_directory_path() / "AsteroidsSoundOverrideTests";
    std::filesystem::remove_all(folder);

    Asteroids::Sounds sounds = Asteroids::MakeSounds();
    const f32 thrustSeconds = sounds.Thrust.GetDurationSeconds();
    const f32 firePeak = Peak(sounds.Fire);
    const f32 saucerPeak = Peak(sounds.SaucerSmall);
    Check(sounds.Music.IsEmpty() && sounds.Ambience.IsEmpty(), "no music/ambience by default");
    Check(!sounds.SaucerLarge.IsEmpty() && !sounds.SaucerSmall.IsEmpty() &&
              !sounds.SaucerFire.IsEmpty(),
          "saucer sounds are generated");
    Check(std::abs(sounds.SaucerLarge.GetDurationSeconds() - 1.0f) < 0.01f,
          "saucer loop is one second (whole wobbles)");

    // A missing folder changes nothing.
    Check(Asteroids::LoadOverrides(sounds, folder).empty(), "missing folder: no overrides");

    std::filesystem::create_directories(folder);
    WriteWav(folder / "fire.wav", 0.2f);
    WriteWav(folder / "music.wav", 1.5f);
    WriteWav(folder / "saucer_small.wav", 0.5f);
    WriteWav(folder / "unknown_name.wav", 0.1f);                  // not a known name: ignored
    std::ofstream(folder / "thrust.mp3") << "this is not an mp3"; // broken: keeps the generated

    const std::vector<std::string> loaded = Asteroids::LoadOverrides(sounds, folder);
    Check(loaded == std::vector<std::string>{"fire", "saucer_small", "music"},
          "fire, saucer_small and music loaded");
    Check(Near(sounds.SaucerSmall.GetDurationSeconds(), 0.5f) &&
              Near(Peak(sounds.SaucerSmall), saucerPeak),
          "saucer_small is the file, at the generated level");
    Check(Near(sounds.Fire.GetDurationSeconds(), 0.2f), "fire is the file (0.2 s)");
    Check(Near(Peak(sounds.Fire), firePeak), "fire scaled to the generated fire's level");
    Check(Near(Peak(sounds.Music), 0.8f), "music normalized to 0.8");
    Check(Near(sounds.Music.GetDurationSeconds(), 1.5f), "music is the file (1.5 s)");
    Check(sounds.Thrust.GetDurationSeconds() == thrustSeconds, "broken thrust.mp3 falls back");
    Check(sounds.Ambience.IsEmpty(), "no ambience file: still empty");

    std::filesystem::remove_all(folder);
    std::printf("%d failed\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
