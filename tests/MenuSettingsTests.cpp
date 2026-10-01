// Tests for the menus and options: settings.txt parsing and writing, menu navigation, which
// title screen page shows when, and the text styles (line font, pixel font, glowing TTF). No
// window, GPU or audio needed: fonts are loaded without a device.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#include <Emerald/Core/Log.h>
#include <Emerald/Renderer/Font.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "Check.h"
#include "Menu.h"
#include "Screens.h"
#include "Settings.h"
#include "Text.h"

namespace {

using Asteroids::Menu;
using Asteroids::MenuAction;
using Asteroids::MenuInput;
using Asteroids::Settings;

void TestSettingsRoundTrip()
{
    Settings settings;
    settings.MasterVolume = 30;
    settings.SfxVolume = 70;
    settings.Fullscreen = true;
    settings.VSync = false;
    settings.ScreenShake = false;
    settings.Particles = false;
    settings.Crt = true;
    settings.RockBounce = true;

    const Settings parsed = Settings::Parse(settings.Serialize());
    Check(parsed.MasterVolume == 30 && parsed.SfxVolume == 70, "volumes survive a round trip");
    Check(parsed.Fullscreen && !parsed.VSync && !parsed.ScreenShake && !parsed.Particles &&
              parsed.Crt && parsed.RockBounce,
          "switches survive a round trip");
}

void TestSettingsBadInput()
{
    const Settings defaults;
    const Settings parsed = Settings::Parse("master_volume = 250\n"      // out of range
                                            "sfx_volume = loud\n"        // not a number
                                            "vsync = maybe\n"            // not on/off
                                            "garbage line\n"             // no '='
                                            "unknown_key = 3\n"          // ignored
                                            "  fullscreen\t=  on \r\n"); // spaces, CRLF
    Check(parsed.MasterVolume == defaults.MasterVolume, "out of range volume keeps the default");
    Check(parsed.SfxVolume == defaults.SfxVolume, "non-numeric volume keeps the default");
    Check(parsed.VSync == defaults.VSync, "bad switch value keeps the default");
    Check(parsed.Fullscreen, "spaces, tabs and CRLF around values are accepted");
    Check(Settings::Parse("").ScreenShake == defaults.ScreenShake, "empty file gives defaults");
    Check(defaults.Particles && Settings::Parse("particles = off\n").Particles == false,
          "particles default on and can be switched off");
    Check(!defaults.Crt && Settings::Parse("crt_effect = on\n").Crt,
          "CRT effect defaults off and can be switched on");
    Check(!defaults.RockBounce && Settings::Parse("rock_bounce = on\n").RockBounce,
          "rock bounce defaults off and can be switched on");
    // A game can pick other defaults (ROGUE turns rock bounce on): the file still wins.
    const Settings rogue{.RockBounce = true};
    Check(Settings::Parse("vsync = off\n", rogue).RockBounce &&
              !Settings::Parse("rock_bounce = off\n", rogue).RockBounce,
          "keys missing from the file keep the game's defaults");
}

void TestSettingsFile()
{
    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "asteroids_settings_test.txt";
    Settings settings;
    settings.SfxVolume = 40;
    Check(settings.Save(file), "settings save to a file");
    Check(Settings::Load(file).SfxVolume == 40, "saved settings load back");
    std::filesystem::remove(file);
    Check(Settings::Load(file).SfxVolume == Settings().SfxVolume, "a missing file gives defaults");
    Check(Settings::Load(file, {.RockBounce = true}).RockBounce,
          "a missing file gives the game's defaults");
}

void TestMenu()
{
    Menu menu({"A", "B", "C"});
    Check(menu.GetSelected() == 0, "menu starts at the first item");

    Check(menu.Update(MenuInput{.Up = true}) == MenuAction::None, "moving is not an action");
    Check(menu.GetSelected() == 2, "up from the first item wraps to the last");
    menu.Update(MenuInput{.Down = true});
    Check(menu.GetSelected() == 0, "down from the last item wraps to the first");
    menu.Update(MenuInput{.Down = true});
    Check(menu.GetSelected() == 1, "down moves one item");

    Check(menu.Update(MenuInput{.Confirm = true}) == MenuAction::Confirm, "confirm is reported");
    Check(menu.Update(MenuInput{.Left = true}) == MenuAction::Left, "left is reported");
    Check(menu.Update(MenuInput{.Right = true}) == MenuAction::Right, "right is reported");
    Check(menu.Update(MenuInput{.Back = true}) == MenuAction::Back, "back is reported");
    Check(menu.Update(MenuInput{}) == MenuAction::None, "no input, no action");
    Check(menu.GetSelected() == 1, "actions keep the selection");

    menu.SetSelected(7);
    Check(menu.GetSelected() == 0, "an invalid selection falls back to the first item");
}

void TestTitlePages()
{
    using namespace Asteroids::TitleScreen;
    const f32 p = kPageSeconds;
    Check(PageAt(0.0f, true) == Page::PressStart, "title starts on the start prompt");
    Check(PageAt(p + 0.1f, true) == Page::HighScores, "then the high scores");
    Check(PageAt(2.0f * p + 0.1f, true) == Page::Controls, "then the controls");
    Check(PageAt(3.0f * p + 0.1f, true) == Page::PressStart, "and around again");
    Check(PageAt(p + 0.1f, false) == Page::Controls, "an empty high score table is skipped");
    Check(PageAt(2.0f * p + 0.1f, false) == Page::PressStart, "two pages without high scores");
}

void TestTextStyles()
{
    namespace Text = Asteroids::Text;
    Emerald::Log::Init({});
    const std::filesystem::path fonts = ASTEROIDS_FONTS_DIR;

    // Default: the line font (letters as wide as tall, 2/3 of that inked).
    Check(Text::UsesLineFont(), "the line font is the default");
    Check(std::abs(Text::Width("AB", 12.0f) - 20.0f) < 0.01f, "line font width");

    // Pixel font: Press Start 2P baked at 8 px, drawn at whole multiples, rounded a little down.
    std::optional<Emerald::Font> pixel =
        Emerald::Font::Load(nullptr, fonts / "PressStart2P-Regular.ttf",
                            {.Size = 8.0f,
                             .Ranges = {Text::kCapitalsAndSymbols},
                             .Oversample = 1,
                             .Filter = Emerald::TextureFilter::Nearest});
    Check(pixel.has_value(), "Press Start 2P loads");
    if (pixel) {
        Text::SetStyle({.Fonts = {&*pixel}, .PixelSizes = true});
        Check(std::abs(Text::Width("AB", 16.0f) - 32.0f) < 0.01f, "16 units: 2x (16 px letters)");
        Check(std::abs(Text::Width("AB", 20.0f) - 32.0f) < 0.01f, "20 units rounds down to 2x");
        Check(std::abs(Text::Width("AB", 5.0f) - 16.0f) < 0.01f, "never smaller than 1x");
        Check(Text::Width("ab", 16.0f) == Text::Width("AB", 16.0f), "drawn in capitals");

        Emerald::Renderer2D r;
        r.Begin(Emerald::Mat4::OrthoPixelSpace(1280.0f, 720.0f));
        Text::Draw(r, "ab c", {10.0f, 10.0f}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f});
        r.End();
        Check(r.GetSpriteCount() == 3, "one sprite per letter (lower case uses the capitals)");
    }

    // Glowing TTF on the line font's grid: same widths as the line font, a halo, additive.
    std::optional<Emerald::Font> smooth =
        Emerald::Font::Load(nullptr, fonts / "ShareTechMono-Regular.ttf",
                            {.Size = 32.0f, .Ranges = {Text::kCapitalsAndSymbols}});
    Check(smooth.has_value(), "Share Tech Mono loads");
    if (smooth) {
        Text::SetStyle({.Fonts = {&*smooth}, .Monospace = true, .Additive = true, .Halo = 0.2f});
        Check(std::abs(Text::Width("AB", 12.0f) - 20.0f) < 0.01f, "monospace keeps line widths");

        Emerald::Renderer2D r;
        r.Begin(Emerald::Mat4::OrthoPixelSpace(1280.0f, 720.0f));
        Text::DrawCentered(r, "AB", 640.0f, 10.0f, 24.0f, {1.0f, 1.0f, 1.0f, 1.0f});
        const bool restored = r.GetBlendMode() == Emerald::BlendMode::Alpha;
        Text::Draw(r, "AB", {10.0f, 50.0f}, 24.0f, {1.0f, 1.0f, 1.0f, 1.0f}, false);
        r.End();
        Check(restored, "the blend mode is restored after additive text");
        Check(r.GetSpriteCount() == 2 * 9 + 2, "8 halo copies + the letters; halo can be off");
        Check(r.GetCommands().size() >= 1 &&
                  r.GetCommands()[0].Blend == Emerald::BlendMode::Additive,
              "drawn additively");
    }
    Text::SetStyle({});
}

} // namespace

int main()
{
    TestSettingsRoundTrip();
    TestSettingsBadInput();
    TestSettingsFile();
    TestMenu();
    TestTitlePages();
    TestTextStyles();
    std::printf("%d failure(s)\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
