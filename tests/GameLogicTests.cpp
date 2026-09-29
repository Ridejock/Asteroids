// Tests for the pure game logic: the high score table (ordering, top 10, text format) and the
// saucer's aiming math. No window, GPU or audio needed.

#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"
#include "HighScores.h"
#include "Playfield.h"
#include "Saucer.h"

namespace {

using Asteroids::HighScoreTable;

bool Near(f32 a, f32 b)
{
    return std::abs(a - b) < 1e-4f;
}

void TestInsertAndSort()
{
    HighScoreTable table;
    Check(!table.Qualifies(0), "a score of 0 never qualifies");
    Check(table.Qualifies(10), "anything qualifies for an empty table");
    Check(table.Insert("BBB", 500) == 0, "first entry is rank 0");
    Check(table.Insert("AAA", 900) == 0, "a better score goes on top");
    Check(table.Insert("CCC", 100) == 2, "a worse one at the bottom");
    Check(table.Insert("DDD", 500) == 2, "a tie goes below the older entry");
    const auto& e = table.GetEntries();
    Check(e.size() == 4 && e[0].Initials == "AAA" && e[1].Initials == "BBB" &&
              e[2].Initials == "DDD" && e[3].Initials == "CCC",
          "sorted best first");
    Check(table.GetBest() == 900, "GetBest is the top score");
}

void TestTruncate()
{
    HighScoreTable table;
    for (u32 i = 1; i <= 12; ++i)
        table.Insert("ABC", i * 100);
    Check(table.GetEntries().size() == HighScoreTable::kMaxEntries, "only 10 are kept");
    Check(table.GetEntries().back().Score == 300, "the lowest two fell off");
    Check(!table.Qualifies(300), "equal to the 10th place does not qualify");
    Check(table.Qualifies(301), "better than the 10th place qualifies");
    Check(table.Insert("XYZ", 50) == HighScoreTable::kMaxEntries, "too low: not inserted");
    Check(table.Insert("XYZ", 5000) == 0 && table.GetEntries().back().Score == 400,
          "a new best pushes the 10th out");
}

void TestInitials()
{
    Check(HighScoreTable::CleanInitials("abc") == "ABC", "lower case becomes upper case");
    Check(HighScoreTable::CleanInitials("A") == "A  ", "short initials are padded");
    Check(HighScoreTable::CleanInitials("ABCD") == "ABC", "long initials are cut");
    Check(HighScoreTable::CleanInitials("A#1") == "A  ", "other characters become spaces");
}

void TestSerializeAndParse()
{
    HighScoreTable table;
    table.Insert("ABC", 12340);
    table.Insert("E A", 9870);
    table.Insert("  Z", 10);
    const std::string text = table.Serialize();
    Check(text == "ABC 12340\nE A 9870\n  Z 10\n", "serialized one line per entry");

    const HighScoreTable back = HighScoreTable::Parse(text);
    Check(back.GetEntries().size() == 3 && back.GetEntries()[1].Initials == "E A" &&
              back.GetEntries()[2].Initials == "  Z" && back.GetEntries()[0].Score == 12340,
          "round trip keeps initials (with spaces) and scores");

    // Damaged or edited files: bad lines are skipped, the rest is sorted and cut to 10.
    const HighScoreTable messy = HighScoreTable::Parse("LOW 5\r\n"   // Windows line ending
                                                       "garbage\n"   // not an entry
                                                       "\n"          // empty line
                                                       "TOP 99999\n" // out of order
                                                       "BAD 12x\n"   // junk after the number
                                                       "NEG -5\n"    // not a number
                                                       "BIG 99999999999999999\n" // too big for u32
                                                       "abc 70\n" // lower case gets cleaned
                                                       "EMPTY\n"
                                                       "NO_SPACE123\n"
                                                       "END 42"); // no final newline
    const auto& e = messy.GetEntries();
    Check(e.size() == 4, "four valid lines survive");
    Check(e.size() == 4 && e[0].Initials == "TOP" && e[1].Initials == "ABC" &&
              e[2].Initials == "END" && e[3].Initials == "LOW" && e[3].Score == 5,
          "valid lines sorted, cleaned, CR stripped");

    std::string many;
    for (u32 i = 0; i < 15; ++i)
        many += "AAA " + std::to_string(i + 1) + "\n";
    Check(HighScoreTable::Parse(many).GetEntries().size() == 10, "a long file is cut to 10");
    Check(HighScoreTable::Parse("").GetEntries().empty(), "empty text: empty table");
}

void TestSaucerAim()
{
    using Asteroids::AimDirection;
    using Asteroids::kPlayfieldSize;
    using Emerald::Vec2;

    const Vec2 right = AimDirection({100.0f, 300.0f}, {400.0f, 300.0f}, 0.0f);
    Check(Near(right.x, 1.0f) && Near(right.y, 0.0f), "aims straight at the target");

    // Across the wrap: from near the left edge to near the right edge is a short hop left.
    const Vec2 left = AimDirection({10.0f, 300.0f}, {kPlayfieldSize.x - 10.0f, 300.0f}, 0.0f);
    Check(Near(left.x, -1.0f) && Near(left.y, 0.0f), "aims the short way around the edge");

    // +HalfPi turns right (+x) into down (+y, since +Y is down on screen).
    const Vec2 turned = AimDirection({100.0f, 300.0f}, {400.0f, 300.0f}, Emerald::HalfPi);
    Check(Near(turned.x, 0.0f) && Near(turned.y, 1.0f), "error angle rotates the aim");

    const f32 early = Asteroids::SmallSaucerAimError(0);
    const f32 mid = Asteroids::SmallSaucerAimError(20000);
    const f32 late = Asteroids::SmallSaucerAimError(40000);
    Check(early > mid && mid > late && late > 0.0f, "aim tightens as the score grows");
    Check(Near(late, Asteroids::SmallSaucerAimError(1000000)), "and stops tightening at 40000");

    Check(Asteroids::SmallSaucerChance(0) < 0.2f, "small saucers are rare at first");
    Check(Asteroids::SmallSaucerChance(20000) > Asteroids::SmallSaucerChance(0),
          "and more likely with a higher score");
    Check(Asteroids::SmallSaucerChance(40000) == 1.0f, "and certain from 40000");
}

} // namespace

int main()
{
    TestInsertAndSort();
    TestTruncate();
    TestInitials();
    TestSerializeAndParse();
    TestSaucerAim();
    std::printf("%d failed\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
