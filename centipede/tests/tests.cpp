#include "GameModel.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace centipede
{
// Tests arrange specific collision scenarios without exposing mutable game data.
struct ModelTestAccess
{
    static auto& Mushrooms(GameModel& game) { return game.mushrooms_; }
    static auto& Shots(GameModel& game) { return game.shots_; }
    static auto& Chains(GameModel& game) { return game.chains_; }
    static auto& Player(GameModel& game) { return game.player_; }
    static auto& SpiderOf(GameModel& game) { return game.spider_; }
    static void Spawn(GameModel& game) { game.SpawnSpider(); }
    static void MoveSpider(GameModel& game, float seconds) { game.UpdateSpider(seconds); }
    static void Hit(GameModel& game, std::size_t chain, std::size_t segment)
    { game.HitSegment(chain, segment); }
    static void Damage(GameModel& game) { game.LoseLife(); }
    static void Finish(GameModel& game, State state) { game.Finish(state); }
    static void Score(GameModel& game, int score) { game.score_ = score; }
};
}

using namespace centipede;
using Access = ModelTestAccess;
namespace
{
int checks = 0, failures = 0, cases = 0;
std::filesystem::path directory;

void Check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition)
    { ++failures; std::cerr << "Line " << line << ": " << expression << '\n'; }
}
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)
bool Near(float a, float b) { return std::abs(a - b) < 0.001F; }

template<typename Function> void Test(const char* name, Function function)
{
    ++cases;
    const auto before = failures;
    try { function(); }
    catch (const std::exception& error)
    { ++failures; std::cerr << name << ": " << error.what() << '\n'; }
    std::cout << (failures == before ? "PASS " : "FAIL ") << name << '\n';
}

GameModel Fresh()
{
    GameModel game{directory / ("case-" + std::to_string(cases) + ".txt"), 42};
    game.Start();
    return game;
}

void ClearField(GameModel& game)
{
    Access::Mushrooms(game).clear();
    Access::Shots(game).clear();
    Access::SpiderOf(game).reset();
}
}

int main()
{
    directory = std::filesystem::temp_directory_path() /
        ("centipede-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);

    Test("Menu does not advance gameplay", []
    {
        GameModel game{directory / "menu.txt", 42};
        const auto x = game.GetPlayer().position.x;
        game.Update(0.05F, {1, 0, true});
        CHECK(game.GetState() == State::Menu);
        CHECK(Near(game.GetPlayer().position.x, x));
        CHECK(game.Shots().empty());
    });
    Test("Twelve segments and three lives at start", []
    {
        auto game = Fresh();
        CHECK(game.Chains().size() == 1);
        CHECK(game.Chains()[0].segments.size() == 12);
        CHECK(game.GetPlayer().lives == 3);
    });
    Test("Fixed mushroom field is valid and clears starting position", []
    {
        auto game = Fresh();
        CHECK(game.Mushrooms().size() == 80);
        for (std::size_t i = 0; i < game.Mushrooms().size(); ++i)
        {
            const auto box = game.Mushrooms()[i].Bounds();
            CHECK(box.x >= 0 && box.x + box.width <= Width);
            CHECK(box.y >= FieldTop && box.y + box.height < PlayerFloor);
            CHECK(!Intersects(box, game.GetPlayer().Bounds()));
            for (std::size_t j = i + 1; j < game.Mushrooms().size(); ++j)
                CHECK(!Intersects(box, game.Mushrooms()[j].Bounds()));
        }
        const auto original = game.Mushrooms();
        Access::Mushrooms(game).clear();
        game.Start();
        for (std::size_t i = 0; i < original.size(); ++i)
        {
            CHECK(Near(original[i].position.x, game.Mushrooms()[i].position.x));
            CHECK(Near(original[i].position.y, game.Mushrooms()[i].position.y));
        }
    });
    Test("Player clamps to every movement boundary", []
    {
        auto game = Fresh();
        Access::Player(game).position = {1, PlayerTop + 1};
        game.Update(0.05F, {-1, -1, false});
        CHECK(Near(game.GetPlayer().position.x, 0));
        CHECK(Near(game.GetPlayer().position.y, PlayerTop));
        Access::Player(game).position = {Width - 39, PlayerFloor - 25};
        game.Update(0.05F, {1, 1, false});
        CHECK(Near(game.GetPlayer().position.x, Width - 38));
        CHECK(Near(game.GetPlayer().position.y, PlayerFloor - 24));
    });
    Test("Diagonal movement has the same speed as horizontal movement", []
    {
        auto a = Fresh(), b = Fresh();
        Access::Player(a).position = {300, 700};
        Access::Player(b).position = {300, 700};
        a.Update(0.05F, {1, 0, false});
        b.Update(0.05F, {1, -1, false});
        CHECK(Near(a.GetPlayer().position.x - 300,
              std::hypot(b.GetPlayer().position.x - 300, b.GetPlayer().position.y - 700)));
    });
    Test("Pause freezes movement, firing and timers", []
    {
        auto game = Fresh();
        game.TogglePause();
        game.Update(0.05F, {1, -1, true});
        CHECK(game.GetState() == State::Paused);
        CHECK(Near(game.GetPlayer().position.x, 581));
        CHECK(game.Shots().empty());
        game.TogglePause();
        game.Update(0.05F, {1, 0, true});
        CHECK(game.GetState() == State::Playing);
        CHECK(game.Shots().size() == 1);
    });
    Test("No more than three simultaneous lasers", []
    {
        auto game = Fresh();
        ClearField(game);
        for (int tick = 0; tick < 12; ++tick)
        {
            game.Update(0.05F, {0, 0, true});
            CHECK(game.Shots().size() <= 3);
        }
        CHECK(game.Shots().size() == 3);
    });
    Test("Mushrooms require four hits and award ten points", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Mushrooms(game).push_back({{300, 300}, 4});
        for (int hit = 0; hit < 4; ++hit)
        {
            Access::Shots(game).push_back({{307, 330}, true});
            game.Update(0.05F);
            if (hit < 3)
            { CHECK(game.Mushrooms()[0].health == 3 - hit); CHECK(game.Score() == 0); }
        }
        CHECK(game.Mushrooms().empty());
        CHECK(game.Score() == 10);
    });
    Test("Swept laser hits nearest target even across a thin obstacle", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Mushrooms(game) = {{{300, 300}, 4}, {{300, 325}, 4}};
        Access::Shots(game).push_back({{307, 350}, true});
        game.Update(0.05F);
        CHECK(game.Mushrooms()[0].health == 4);
        CHECK(game.Mushrooms()[1].health == 3);
        CHECK(game.Shots().empty());
    });
    Test("Interior segment hit splits train and creates a mushroom", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Hit(game, 0, 5);
        CHECK(game.Chains().size() == 2);
        CHECK(game.Chains()[0].segments.size() == 5);
        CHECK(game.Chains()[1].segments.size() == 6);
        CHECK(game.Mushrooms().size() == 1);
        CHECK(Near(game.Mushrooms()[0].position.x, 480));
    });
    Test("Removing head promotes next segment and tail leaves front", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Hit(game, 0, 0);
        CHECK(game.Chains().size() == 1);
        CHECK(game.Chains()[0].segments.size() == 11);
        CHECK(Near(game.Chains()[0].segments[0].x, 576));
        Access::Hit(game, 0, 10);
        CHECK(game.Chains().size() == 1);
        CHECK(game.Chains()[0].segments.size() == 10);
    });
    Test("Screen edge reverses head and drops one row", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Chains(game) = {{{{1176, 100}, {1152, 100}}, 1, 0}};
        game.Update(0.01F);
        CHECK(game.Chains()[0].direction == -1);
        CHECK(Near(game.Chains()[0].segments[0].y, 124));
        CHECK(Near(game.Chains()[0].segments[1].x, 1176));
    });
    Test("Mushroom reverses and lowers centipede head", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Chains(game) = {{{{300, 300}}, 1, 0}};
        Access::Mushrooms(game).push_back({{324, 300}, 4});
        game.Update(0.01F);
        CHECK(game.Chains()[0].direction == -1);
        CHECK(Near(game.Chains()[0].segments[0].y, 324));
    });
    Test("Last segment hit wins and records score only once", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Chains(game) = {{{{300, 300}}, 1, 0.10F}};
        Access::Shots(game).push_back({{307, 330}, true});
        game.Update(0.05F);
        CHECK(game.GetState() == State::Won);
        CHECK(game.Score() == 100);
        CHECK(game.Scores().Entries().size() == 1);
        for (int tick = 0; tick < 10; ++tick) game.Update(0.05F);
        CHECK(game.Scores().Entries().size() == 1);
    });
    Test("Centipede reaching the bottom ends the game", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::Chains(game) = {{{{1176, 724}}, 1, 0}};
        game.Update(0.01F);
        CHECK(game.GetState() == State::Lost);
    });
    Test("Spider collision loses one life and grants protection", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::SpiderOf(game) = Spider{game.GetPlayer().position, 0, 1, 1, 10, false};
        game.Update(0.01F);
        CHECK(game.GetPlayer().lives == 2);
        CHECK(game.GetPlayer().invulnerability > 0);
        CHECK(!game.GetSpider());
        Access::SpiderOf(game) = Spider{game.GetPlayer().position, 0, 1, 1, 10, false};
        game.Update(0.01F);
        CHECK(game.GetPlayer().lives == 2);
    });
    Test("Losing all lives defeats and restart restores the game", []
    {
        auto game = Fresh();
        Access::Damage(game); Access::Damage(game); Access::Damage(game);
        CHECK(game.GetState() == State::Lost);
        game.Start();
        CHECK(game.GetState() == State::Playing);
        CHECK(game.GetPlayer().lives == 3);
        CHECK(game.Score() == 0);
        CHECK(game.Mushrooms().size() == 80);
    });
    Test("Spider enters a side facing inward at a valid height", []
    {
        auto game = Fresh();
        for (int run = 0; run < 100; ++run)
        {
            Access::Spawn(game);
            const auto spider = *game.GetSpider();
            CHECK(Near(spider.position.x, spider.direction == 1 ? -38.0F : Width));
            CHECK(spider.position.y >= 520 && spider.position.y <= 695);
        }
    });
    Test("Spider stays vertically bounded and eventually exits", []
    {
        auto game = Fresh();
        for (int run = 0; run < 100; ++run)
        {
            Access::Spawn(game);
            for (int tick = 0; tick < 3000 && game.GetSpider(); ++tick)
            {
                Access::MoveSpider(game, 0.01F);
                if (game.GetSpider())
                    CHECK(game.GetSpider()->position.y >= 520 && game.GetSpider()->position.y <= 738);
            }
            CHECK(!game.GetSpider());
        }
    });
    Test("Spider waits five seconds before respawning", []
    {
        auto game = Fresh();
        Access::SpiderOf(game) = Spider{{Width - 1, 600}, 0, 1, 1, 0, true};
        Access::MoveSpider(game, 0.05F);
        CHECK(!game.GetSpider());
        for (int tick = 0; tick < 99; ++tick) Access::MoveSpider(game, 0.05F);
        CHECK(!game.GetSpider());
        Access::MoveSpider(game, 0.051F);
        CHECK(game.GetSpider().has_value());
    });
    Test("Hitting spider awards 300 points", []
    {
        auto game = Fresh();
        ClearField(game);
        Access::SpiderOf(game) = Spider{{300, 550}, 0, 1, 1, 10, false};
        Access::Shots(game).push_back({{307, 590}, true});
        game.Update(0.05F);
        CHECK(!game.GetSpider());
        CHECK(game.Score() == 300);
    });
    Test("Invalid update times are rejected", []
    {
        auto game = Fresh();
        for (const auto time : {-1.0F, std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()})
        {
            bool threw = false;
            try { game.Update(time); } catch (const std::invalid_argument&) { threw = true; }
            CHECK(threw);
        }
    });
    Test("Scores persist, sort descending and keep the best five", []
    {
        const auto file = directory / "ranked.txt";
        HighScores scores{file};
        for (const auto value : {100, 900, 300, 800, 600, 200}) CHECK(scores.Record(value));
        HighScores reloaded{file};
        CHECK(reloaded.Entries() == std::vector<int>({900, 800, 600, 300, 200}));
        CHECK(reloaded.Best() == 900);
    });
    Test("Missing or malformed score data is handled", []
    {
        HighScores missing{directory / "missing.txt"};
        CHECK(missing.Entries().empty());
        const auto file = directory / "malformed.txt";
        { std::ofstream output{file}; output << "500\r\nwrong\n-100\n42junk\n300\n9999999999999999\n"; }
        HighScores scores{file};
        CHECK(scores.Entries() == std::vector<int>({500, 300}));
        CHECK(scores.Record(0));
        CHECK(scores.Record(-20));
        CHECK(scores.Entries().size() == 2);
    });
    Test("Saving failure is reported without losing in-memory scores", []
    {
        const auto blockingFile = directory / "blocked";
        { std::ofstream output{blockingFile}; output << "not a directory"; }
        HighScores scores{blockingFile / "scores.txt"};
        CHECK(!scores.Record(100));
        CHECK(scores.Best() == 100);
        auto game = GameModel{blockingFile / "game.txt", 42};
        game.Start();
        Access::Score(game, 250);
        Access::Finish(game, State::Won);
        CHECK(game.SaveFailed());
        CHECK(game.Scores().Best() == 250);
    });

    std::filesystem::remove_all(directory);
    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
