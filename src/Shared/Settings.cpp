#include "Settings.h"

#include <charconv>
#include <fstream>
#include <sstream>

#include <Emerald/Core/Log.h>
#include <Emerald/Core/Paths.h>

#include "GameInfo.h"

namespace Asteroids {

namespace {

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

void ParseBool(std::string_view value, bool& out)
{
    if (value == "on" || value == "true" || value == "1")
        out = true;
    else if (value == "off" || value == "false" || value == "0")
        out = false;
}

void ParsePercent(std::string_view value, u32& out)
{
    u32 parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error == std::errc() && end == value.data() + value.size() && parsed <= 100)
        out = parsed;
}

const char* OnOff(bool value)
{
    return value ? "on" : "off";
}

} // namespace

std::string Settings::Serialize() const
{
    std::ostringstream out;
    out << "master_volume = " << MasterVolume << '\n'
        << "sfx_volume = " << SfxVolume << '\n'
        << "fullscreen = " << OnOff(Fullscreen) << '\n'
        << "vsync = " << OnOff(VSync) << '\n'
        << "screen_shake = " << OnOff(ScreenShake) << '\n'
        << "particles = " << OnOff(Particles) << '\n';
    return out.str();
}

Settings Settings::Parse(std::string_view text)
{
    Settings settings;
    while (!text.empty()) {
        const usize end = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);

        const usize equals = line.find('=');
        if (equals == std::string_view::npos)
            continue;
        const std::string_view key = Trim(line.substr(0, equals));
        const std::string_view value = Trim(line.substr(equals + 1));
        if (key == "master_volume")
            ParsePercent(value, settings.MasterVolume);
        else if (key == "sfx_volume")
            ParsePercent(value, settings.SfxVolume);
        else if (key == "fullscreen")
            ParseBool(value, settings.Fullscreen);
        else if (key == "vsync")
            ParseBool(value, settings.VSync);
        else if (key == "screen_shake")
            ParseBool(value, settings.ScreenShake);
        else if (key == "particles")
            ParseBool(value, settings.Particles);
    }
    return settings;
}

std::filesystem::path Settings::DefaultPath()
{
    const std::filesystem::path folder =
        Emerald::Paths::GetPrefPath(GameInfo::kOrganization, GameInfo::kFileName);
    return folder.empty() ? folder : (folder / "settings.txt").make_preferred();
}

Settings Settings::Load(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (file.empty() || !in)
        return {};
    std::stringstream buffer;
    buffer << in.rdbuf();
    return Parse(buffer.str());
}

bool Settings::Save(const std::filesystem::path& file) const
{
    if (file.empty())
        return false;
    // Temporary file + rename, like the high scores: a crash mid-write keeps the old file.
    std::filesystem::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << Serialize();
        if (!out) {
            EM_WARN("Could not write settings to {}", temp.string());
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, file, error);
    if (error) {
        EM_WARN("Could not save settings to {}: {}", file.string(), error.message());
        return false;
    }
    return true;
}

} // namespace Asteroids
