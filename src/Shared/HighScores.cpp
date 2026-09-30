#include "HighScores.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

#include <Emerald/Core/Log.h>

namespace Asteroids {

bool HighScoreTable::Qualifies(u32 score) const
{
    if (score == 0)
        return false;
    return m_Entries.size() < kMaxEntries || score > m_Entries.back().Score;
}

usize HighScoreTable::Insert(std::string_view initials, u32 score)
{
    if (!Qualifies(score))
        return kMaxEntries;
    // First entry with a lower score: the new one goes right before it.
    const auto it = std::find_if(m_Entries.begin(), m_Entries.end(),
                                 [score](const HighScore& e) { return e.Score < score; });
    const usize rank = static_cast<usize>(it - m_Entries.begin());
    m_Entries.insert(it, HighScore{CleanInitials(initials), score});
    if (m_Entries.size() > kMaxEntries)
        m_Entries.resize(kMaxEntries);
    return rank;
}

std::string HighScoreTable::Serialize() const
{
    std::string text;
    for (const HighScore& e : m_Entries)
        text += e.Initials + " " + std::to_string(e.Score) + "\n";
    return text;
}

HighScoreTable HighScoreTable::Parse(std::string_view text)
{
    HighScoreTable table;
    while (!text.empty()) {
        // Take one line (without "\n", and without the "\r" a Windows editor might add).
        const usize end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        // "ABC 12340": 3 initials, a space, then only digits.
        if (line.size() < kInitialsLength + 2 || line[kInitialsLength] != ' ')
            continue;
        const std::string_view digits = line.substr(kInitialsLength + 1);
        u32 score = 0;
        const auto [ptr, error] =
            std::from_chars(digits.data(), digits.data() + digits.size(), score);
        if (error != std::errc() || ptr != digits.data() + digits.size())
            continue; // not a number, too big, or junk after it
        table.Insert(line.substr(0, kInitialsLength), score); // sorts and keeps the best 10
    }
    return table;
}

HighScoreTable HighScoreTable::Load(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        EM_INFO("No high scores yet ({} not found)", file.string());
        return {};
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    HighScoreTable table = Parse(buffer.str());
    EM_INFO("Loaded {} high scores from {}", table.m_Entries.size(), file.string());
    return table;
}

bool HighScoreTable::Save(const std::filesystem::path& file) const
{
    // Write a temporary file and then swap it in, so a crash mid-write can't eat the old table.
    std::filesystem::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << Serialize();
        if (!out) {
            EM_WARN("Could not write high scores to {}", temp.string());
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, file, error); // replaces the old file
    if (error) {
        EM_WARN("Could not save high scores to {}: {}", file.string(), error.message());
        return false;
    }
    EM_INFO("Saved high scores to {}", file.string());
    return true;
}

std::string HighScoreTable::CleanInitials(std::string_view initials)
{
    std::string clean(kInitialsLength, ' ');
    for (usize i = 0; i < kInitialsLength && i < initials.size(); ++i) {
        char c = initials[i];
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        clean[i] = c >= 'A' && c <= 'Z' ? c : ' ';
    }
    return clean;
}

} // namespace Asteroids
