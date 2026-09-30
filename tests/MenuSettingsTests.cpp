// Tests for the menus and options: settings.txt parsing and writing, menu navigation, and which
// title screen page shows when. No window, GPU or audio needed.

#include <cstdio>
#include <filesystem>
#include <string>

#include "Check.h"
#include "Menu.h"
#include "Screens.h"
#include "Settings.h"

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

    const Settings parsed = Settings::Parse(settings.Serialize());
    Check(parsed.MasterVolume == 30 && parsed.SfxVolume == 70, "volumes survive a round trip");
    Check(parsed.Fullscreen && !parsed.VSync && !parsed.ScreenShake,
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

} // namespace

int main()
{
    TestSettingsRoundTrip();
    TestSettingsBadInput();
    TestSettingsFile();
    TestMenu();
    TestTitlePages();
    std::printf("%d failure(s)\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
