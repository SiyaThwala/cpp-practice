#include "GameModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace centipede
{
bool Intersects(Box a, Box b)
{
    return a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

GameModel::GameModel(std::filesystem::path scoresFile, std::uint32_t seed)
    : random_{seed}, scores_{std::move(scoresFile)}
{
    Reset();
}

float GameModel::Random(float minimum, float maximum)
{
    return std::uniform_real_distribution<float>{minimum, maximum}(random_);
}

void GameModel::CreateField()
{
    // One shuffled layout with its own fixed seed: identical on every restart.
    std::mt19937 layoutRandom{3024048};
    std::vector<Point> candidates;
    for (int row = 0; row < 20; ++row)
        for (int column = 0; column < 35; ++column)
        {
            const Point position{32.0F + column * 32.0F, 146.0F + row * 30.0F};
            if (!Intersects({position.x, position.y, 20.0F, 20.0F},
                            {Width / 2.0F - 65.0F, 710.0F, 130.0F, 56.0F}))
                candidates.push_back(position);
        }
    std::shuffle(candidates.begin(), candidates.end(), layoutRandom);
    std::uniform_real_distribution<float> jitter{-3.0F, 3.0F};
    for (std::size_t index = 0; index < 80; ++index)
    {
        auto position = candidates[index];
        position.x += jitter(layoutRandom);
        position.y += jitter(layoutRandom);
        mushrooms_.push_back({position, 4});
    }
}

void GameModel::Reset()
{
    score_ = 0;
    saveFailed_ = false;
    player_ = Player{};
    mushrooms_.clear();
    shots_.clear();
    chains_.clear();
    spider_.reset();
    fireCooldown_ = 0.0F;
    spiderCooldown_ = 2.0F;
    CreateField();
    Chain chain;
    for (int index = 0; index < 12; ++index)
        chain.segments.push_back({600.0F - index * Cell, FieldTop});
    chains_.push_back(std::move(chain));
}

void GameModel::Start()
{
    Reset();
    state_ = State::Playing;
}

void GameModel::TogglePause()
{
    if (state_ == State::Playing) state_ = State::Paused;
    else if (state_ == State::Paused) state_ = State::Playing;
}

void GameModel::Update(float seconds, Input input)
{
    if (!std::isfinite(seconds) || seconds < 0.0F)
        throw std::invalid_argument{"Update time must be finite and nonnegative."};
    if (state_ != State::Playing || seconds == 0.0F) return;
    seconds = std::min(seconds, 0.05F);
    UpdatePlayer(seconds, input);
    UpdateShots(seconds);
    if (chains_.empty()) { Finish(State::Won); return; }
    UpdateChains(seconds);
    if (state_ != State::Playing) return;
    UpdateSpider(seconds);
    CheckPlayerHits();
}

void GameModel::UpdatePlayer(float seconds, Input input)
{
    auto x = std::clamp(input.x, -1.0F, 1.0F);
    auto y = std::clamp(input.y, -1.0F, 1.0F);
    const auto magnitude = std::sqrt(x * x + y * y);
    if (magnitude > 1.0F) { x /= magnitude; y /= magnitude; }
    player_.position.x = std::clamp(player_.position.x + x * 310.0F * seconds,
                                    0.0F, Width - 38.0F);
    player_.position.y = std::clamp(player_.position.y + y * 310.0F * seconds,
                                    PlayerTop, PlayerFloor - 24.0F);
    player_.invulnerability = std::max(0.0F, player_.invulnerability - seconds);
    fireCooldown_ = std::max(0.0F, fireCooldown_ - seconds);
    if (input.fire && fireCooldown_ <= 0.0F && shots_.size() < 3)
    {
        shots_.push_back({{player_.position.x + 17.0F, player_.position.y - 14.0F}, true});
        fireCooldown_ = 0.16F;
    }
}

void GameModel::UpdateShots(float seconds)
{
    enum class Target { None, Mushroom, Spider, Segment };
    for (auto& shot : shots_)
    {
        const auto oldY = shot.position.y;
        shot.position.y -= 700.0F * seconds;
        const Box sweep{shot.position.x, shot.position.y, 4.0F,
                        oldY - shot.position.y + 14.0F};
        auto nearest = std::numeric_limits<float>::max();
        auto target = Target::None;
        std::size_t first = 0, second = 0;
        const auto consider = [&](Box box, Target candidate, std::size_t a, std::size_t b)
        {
            if (!Intersects(sweep, box)) return;
            const auto distance = std::max(0.0F, oldY - box.y - box.height);
            if (distance < nearest)
            { nearest = distance; target = candidate; first = a; second = b; }
        };
        for (std::size_t i = 0; i < mushrooms_.size(); ++i)
            consider(mushrooms_[i].Bounds(), Target::Mushroom, i, 0);
        if (spider_) consider(spider_->Bounds(), Target::Spider, 0, 0);
        for (std::size_t i = 0; i < chains_.size(); ++i)
            for (std::size_t j = 0; j < chains_[i].segments.size(); ++j)
            {
                const auto position = chains_[i].segments[j];
                consider({position.x, position.y, SegmentSize, SegmentSize}, Target::Segment, i, j);
            }
        switch (target)
        {
        case Target::Mushroom:
            if (--mushrooms_[first].health == 0)
            { mushrooms_.erase(mushrooms_.begin() + first); score_ += 10; }
            break;
        case Target::Spider: spider_.reset(); spiderCooldown_ = 5.0F; score_ += 300; break;
        case Target::Segment: HitSegment(first, second); score_ += 100; break;
        case Target::None: break;
        }
        shot.active = target == Target::None && shot.position.y + 14.0F >= FieldTop;
    }
    std::erase_if(shots_, [](const Shot& shot) { return !shot.active; });
}

void GameModel::HitSegment(std::size_t chainIndex, std::size_t segmentIndex)
{
    auto& chain = chains_[chainIndex];
    const auto hit = chain.segments[segmentIndex];
    const Box hitBox{hit.x, hit.y, SegmentSize, SegmentSize};
    const auto overlapping = std::any_of(mushrooms_.begin(), mushrooms_.end(),
        [&](const Mushroom& mushroom) { return Intersects(hitBox, mushroom.Bounds()); });
    if (!overlapping) mushrooms_.push_back({hit, 4});
    Chain rear;
    rear.direction = chain.direction;
    rear.untilStep = chain.untilStep;
    rear.segments.assign(chain.segments.begin() + segmentIndex + 1, chain.segments.end());
    chain.segments.erase(chain.segments.begin() + segmentIndex, chain.segments.end());
    if (chain.segments.empty()) chains_.erase(chains_.begin() + chainIndex);
    if (!rear.segments.empty()) chains_.push_back(std::move(rear));
}

void GameModel::UpdateChains(float seconds)
{
    for (auto& chain : chains_)
    {
        chain.untilStep -= seconds;
        if (chain.untilStep > 0.0F) continue;
        chain.untilStep += 0.10F;
        auto next = chain.segments.front();
        next.x += chain.direction * Cell;
        const Box nextBox{next.x, next.y, SegmentSize, SegmentSize};
        const auto mushroomHit = std::any_of(mushrooms_.begin(), mushrooms_.end(),
            [&](const Mushroom& mushroom) { return Intersects(nextBox, mushroom.Bounds()); });
        if (next.x < 0.0F || next.x + SegmentSize > Width || mushroomHit)
        {
            next = chain.segments.front();
            next.y += Cell;
            chain.direction *= -1;
        }
        for (std::size_t i = chain.segments.size() - 1; i > 0; --i)
            chain.segments[i] = chain.segments[i - 1];
        chain.segments.front() = next;
        if (next.y + SegmentSize >= PlayerFloor) { Finish(State::Lost); return; }
    }
}

void GameModel::SpawnSpider()
{
    const auto fromLeft = Random(0.0F, 1.0F) < 0.5F;
    spider_ = Spider{{fromLeft ? -38.0F : Width, Random(520.0F, 695.0F)},
                      75.0F, fromLeft ? 1 : -1, Random(0.35F, 1.05F),
                      Random(6.0F, 8.0F), false};
}

void GameModel::UpdateSpider(float seconds)
{
    if (!spider_)
    {
        spiderCooldown_ -= seconds;
        if (spiderCooldown_ <= 0.0F) SpawnSpider();
        return;
    }
    auto& spider = *spider_;
    spider.untilDecision -= seconds;
    spider.untilExit -= seconds;
    if (!spider.exiting && spider.untilExit <= 0.0F)
    { spider.exiting = true; spider.direction = Random(0.0F, 1.0F) < 0.5F ? -1 : 1; }
    if (spider.untilDecision <= 0.0F)
    {
        spider.untilDecision = Random(0.35F, 1.05F);
        spider.verticalSpeed = Random(35.0F, 90.0F) * (Random(0.0F, 1.0F) < 0.5F ? -1.0F : 1.0F);
        if (!spider.exiting && spider.position.x > 76.0F && spider.position.x < Width - 114.0F &&
            Random(0.0F, 1.0F) < 0.35F) spider.direction *= -1;
    }
    spider.position.x += spider.direction * 120.0F * seconds;
    spider.position.y += spider.verticalSpeed * seconds;
    if (spider.position.y < 520.0F)
    { spider.position.y = 520.0F; spider.verticalSpeed = std::abs(spider.verticalSpeed); }
    if (spider.position.y > PlayerFloor - 28.0F)
    { spider.position.y = PlayerFloor - 28.0F; spider.verticalSpeed = -std::abs(spider.verticalSpeed); }
    if (spider.position.x + 38.0F <= 0.0F || spider.position.x >= Width)
    { spider_.reset(); spiderCooldown_ = 5.0F; return; }
    for (std::size_t i = 0; i < mushrooms_.size(); ++i)
        if (Intersects(spider.Bounds(), mushrooms_[i].Bounds()) && Random(0.0F, 1.0F) < seconds * 3.0F)
        { mushrooms_.erase(mushrooms_.begin() + i); break; }
}

void GameModel::CheckPlayerHits()
{
    if (player_.invulnerability > 0.0F) return;
    if (spider_ && Intersects(player_.Bounds(), spider_->Bounds()))
    { spider_.reset(); spiderCooldown_ = 5.0F; LoseLife(); return; }
    for (const auto& chain : chains_)
        for (const auto& segment : chain.segments)
            if (Intersects(player_.Bounds(), {segment.x, segment.y, SegmentSize, SegmentSize}))
            { LoseLife(); return; }
}

void GameModel::LoseLife()
{
    --player_.lives;
    if (player_.lives == 0) { Finish(State::Lost); return; }
    player_.position = Player{}.position;
    player_.invulnerability = 2.0F;
}

void GameModel::Finish(State result)
{
    if (state_ != State::Playing) return;
    state_ = result;
    saveFailed_ = !scores_.Record(score_);
}
}
