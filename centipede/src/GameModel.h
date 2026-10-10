#pragma once

#include "HighScores.h"
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace centipede
{
inline constexpr float Width = 1200.0F;
inline constexpr float Height = 800.0F;
inline constexpr float FieldTop = 100.0F;
inline constexpr float PlayerTop = 640.0F;
inline constexpr float PlayerFloor = 766.0F;
inline constexpr float SegmentSize = 20.0F;
inline constexpr float Cell = 24.0F;

struct Point { float x = 0.0F, y = 0.0F; };
struct Box { float x, y, width, height; };
bool Intersects(Box a, Box b);
enum class State { Menu, Playing, Paused, Won, Lost };

/** Input is independent of raylib, allowing gameplay to run in tests. */
struct Input { float x = 0.0F, y = 0.0F; bool fire = false; };
struct Mushroom
{
    Point position;
    int health = 4;
    Box Bounds() const { return {position.x, position.y, SegmentSize, SegmentSize}; }
};
struct Player
{
    Point position{Width / 2.0F - 19.0F, 738.0F};
    int lives = 3;
    float invulnerability = 0.0F;
    Box Bounds() const { return {position.x, position.y, 38.0F, 24.0F}; }
};
struct Shot
{
    Point position;
    bool active = true;
    Box Bounds() const { return {position.x, position.y, 4.0F, 14.0F}; }
};
/** Front element is the head; remaining segments follow its previous cells. */
struct Chain
{
    std::vector<Point> segments;
    int direction = 1;
    float untilStep = 0.10F;
};
struct Spider
{
    Point position;
    float verticalSpeed = 75.0F;
    int direction = 1;
    float untilDecision = 0.6F;
    float untilExit = 6.0F;
    bool exiting = false;
    Box Bounds() const { return {position.x, position.y, 38.0F, 28.0F}; }
};

/** Owns game state, movement, collisions, scoring and completed-game saving. */
class GameModel
{
public:
    explicit GameModel(std::filesystem::path scoresFile,
                       std::uint32_t seed = std::random_device{}());
    void Start();
    void TogglePause();
    /** Advances active gameplay by up to 0.05 seconds; rejects invalid times. */
    void Update(float seconds, Input input = {});
    State GetState() const { return state_; }
    int Score() const { return score_; }
    bool SaveFailed() const { return saveFailed_; }
    const HighScores& Scores() const { return scores_; }
    const Player& GetPlayer() const { return player_; }
    const std::vector<Mushroom>& Mushrooms() const { return mushrooms_; }
    const std::vector<Shot>& Shots() const { return shots_; }
    const std::vector<Chain>& Chains() const { return chains_; }
    const std::optional<Spider>& GetSpider() const { return spider_; }

private:
    friend struct ModelTestAccess;
    void Reset();
    void CreateField();
    void UpdatePlayer(float seconds, Input input);
    void UpdateShots(float seconds);
    void HitSegment(std::size_t chain, std::size_t segment);
    void UpdateChains(float seconds);
    void UpdateSpider(float seconds);
    void SpawnSpider();
    void CheckPlayerHits();
    void LoseLife();
    void Finish(State result);
    float Random(float minimum, float maximum);

    State state_ = State::Menu;
    int score_ = 0;
    bool saveFailed_ = false;
    Player player_;
    std::vector<Mushroom> mushrooms_;
    std::vector<Shot> shots_;
    std::vector<Chain> chains_;
    std::optional<Spider> spider_;
    float fireCooldown_ = 0.0F;
    float spiderCooldown_ = 2.0F;
    std::mt19937 random_;
    HighScores scores_;
};
}
