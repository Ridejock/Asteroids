#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <Emerald/Core/Defines.h>

namespace Asteroids {

struct HighScore {
    std::string Initials; // always 3 characters: A-Z or space
    u32 Score = 0;
    std::string Note; // optional, e.g. "S2 W3" (how far a roguelike run got); A-Z, 0-9, spaces
};

// The arcade's top 10: best score first. Pure logic plus a tiny text format, so it is easy to
// test. The file has one entry per line, initials then score:
//
//   ABC 12340
//   E A 9870      <- spaces are allowed in initials, so they are always exactly 3 characters
//   XYZ 5000 S2 W3 <- an optional note after the score (only written when there is one)
//
// Anything that doesn't look like that is skipped, so a damaged file loses only the bad lines.
class HighScoreTable {
public:
    static constexpr usize kMaxEntries = 10;
    static constexpr usize kInitialsLength = 3;

    // Would this score make it onto the table? (0 never does.)
    [[nodiscard]] bool Qualifies(u32 score) const;
    // Adds the entry in score order and drops whatever falls off the end. A new score goes below
    // equal old ones (first come, first served). Returns its rank (0 = best), or kMaxEntries if
    // it didn't make it.
    usize Insert(std::string_view initials, u32 score, std::string_view note = {});

    [[nodiscard]] const std::vector<HighScore>& GetEntries() const { return m_Entries; }
    [[nodiscard]] u32 GetBest() const { return m_Entries.empty() ? 0 : m_Entries.front().Score; }

    [[nodiscard]] std::string Serialize() const;
    [[nodiscard]] static HighScoreTable Parse(std::string_view text);

    // File helpers; both log what happened. A missing file is just an empty table.
    [[nodiscard]] static HighScoreTable Load(const std::filesystem::path& file);
    bool Save(const std::filesystem::path& file) const;

    // Upper-cases and pads/cuts to 3 characters; anything but A-Z becomes a space.
    [[nodiscard]] static std::string CleanInitials(std::string_view initials);

private:
    std::vector<HighScore> m_Entries; // sorted, best first, at most kMaxEntries
};

} // namespace Asteroids
