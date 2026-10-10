#include "Renderer.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <string>

using namespace centipede;
namespace
{
constexpr Color Ink{6, 12, 23, 255};
constexpr Color Panel{13, 25, 40, 250};
constexpr Color Lime{176, 255, 78, 255};
constexpr Color Cyan{60, 218, 255, 255};
constexpr Color Pink{255, 83, 129, 255};
constexpr Color Muted{133, 158, 179, 255};

void CenterText(const std::string& text, int y, int size, Color color)
{
    DrawText(text.c_str(), (static_cast<int>(Width) - MeasureText(text.c_str(), size)) / 2,
             y, size, color);
}

void MushroomArt(const Mushroom& mushroom)
{
    const auto x = static_cast<int>(mushroom.position.x);
    const auto y = static_cast<int>(mushroom.position.y);
    const Color cap = mushroom.health == 4 ? Color{214, 105, 245, 255} :
                      mushroom.health == 3 ? Color{175, 92, 226, 255} :
                      mushroom.health == 2 ? Color{139, 79, 178, 255} : Color{95, 67, 112, 255};
    DrawCircle(x + 10, y + 9, 14.0F, Fade(cap, 0.07F));
    DrawRectangleRounded({static_cast<float>(x + 7), static_cast<float>(y + 10), 6, 10},
                         0.5F, 4, Color{184, 188, 190, 255});
    DrawEllipse(x + 10, y + 7, 10, 7, cap);
    DrawRectangle(x, y + 7, 20, 5, cap);
    DrawCircle(x + 5, y + 6, 2.0F, Fade(WHITE, 0.7F));
    DrawCircle(x + 13, y + 4, 1.5F, Fade(WHITE, 0.7F));
    for (int dot = 0; dot < mushroom.health; ++dot)
        DrawRectangle(x + 3 + dot * 4, y + 21, 2, 2, Fade(cap, 0.75F));
}

void PlayerArt(Point position, Color color)
{
    const auto x = position.x, y = position.y;
    DrawCircleV({x + 19, y + 15}, 26.0F, Fade(color, 0.08F));
    DrawTriangle({x + 19, y}, {x, y + 24}, {x + 38, y + 24}, color);
    DrawTriangle({x + 19, y + 8}, {x + 11, y + 24}, {x + 27, y + 24}, Ink);
    DrawRectangle(static_cast<int>(x + 17), static_cast<int>(y), 4, 12, WHITE);
    DrawRectangle(static_cast<int>(x + 2), static_cast<int>(y + 20), 7, 5, color);
    DrawRectangle(static_cast<int>(x + 29), static_cast<int>(y + 20), 7, 5, color);
}

void SpiderArt(const Spider& spider, float time)
{
    const Vector2 center{spider.position.x + 19, spider.position.y + 14};
    DrawCircleV(center, 28.0F, Fade(Pink, 0.08F));
    for (int side = -1; side <= 1; side += 2)
        for (int leg = 0; leg < 4; ++leg)
        {
            const auto wave = std::sin(time * 14.0F + leg * 1.8F) * 3.0F;
            const Vector2 knee{center.x + side * 17, center.y - 10 + leg * 7 + wave};
            DrawLineEx(center, knee, 2.0F, Pink);
            DrawLineEx(knee, {center.x + side * 25, center.y - 12 + leg * 9 - wave}, 2.0F, Pink);
        }
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), 12, 10, Pink);
    DrawCircleV({center.x - 4, center.y - 3}, 2.0F, Ink);
    DrawCircleV({center.x + 4, center.y - 3}, 2.0F, Ink);
}
}

void Renderer::Background(float time) const
{
    ClearBackground(Ink);
    DrawRectangleGradientV(0, 0, static_cast<int>(Width), static_cast<int>(Height),
                           Ink, Color{9, 27, 35, 255});
    for (int x = 0; x < static_cast<int>(Width); x += 40)
        DrawLine(x, 88, x, 780, Color{14, 29, 39, 255});
    for (int y = 100; y < 780; y += 40)
        DrawLine(0, y, static_cast<int>(Width), y, Color{14, 29, 39, 255});
    for (int index = 0; index < 60; ++index)
    {
        const auto x = (index * 173 + 41) % 1200;
        const auto y = (index * 107 + 37) % 800;
        const auto alpha = 0.08F + 0.08F * (1.0F + std::sin(time + index));
        DrawCircle(x, y, index % 3 == 0 ? 1.5F : 1.0F, Fade(Cyan, alpha));
    }
    DrawRectangle(0, static_cast<int>(PlayerTop), static_cast<int>(Width), 126, Fade(Cyan, 0.025F));
    DrawLine(0, static_cast<int>(PlayerTop), static_cast<int>(Width), static_cast<int>(PlayerTop),
             Fade(Cyan, 0.18F));
    DrawText("PLAYER AREA", 18, 650, 12, Fade(Cyan, 0.45F));
}

void Renderer::World(const GameModel& game, float time) const
{
    for (const auto& mushroom : game.Mushrooms()) MushroomArt(mushroom);
    for (const auto& chain : game.Chains())
        for (std::size_t index = chain.segments.size(); index-- > 0;)
        {
            const auto segment = chain.segments[index];
            const Vector2 center{segment.x + 10, segment.y + 10};
            const auto color = index == 0 ? Pink : Lime;
            const auto kick = std::sin(time * 16.0F + index) * 2.0F;
            DrawCircleV(center, 16.0F, Fade(color, 0.08F));
            DrawLineEx({center.x - 5, center.y + 7}, {center.x - 11, center.y + 14 + kick},
                       2, Fade(color, 0.7F));
            DrawLineEx({center.x + 5, center.y + 7}, {center.x + 11, center.y + 14 - kick},
                       2, Fade(color, 0.7F));
            DrawCircleV(center, 10, color);
            DrawCircleV({center.x - 3, center.y - 3}, 3, Fade(WHITE, 0.45F));
            if (index == 0)
            {
                const auto facing = static_cast<float>(chain.direction);
                DrawCircleV({center.x + facing * 5, center.y - 4}, 2, Ink);
                DrawLineEx({center.x + facing * 7, center.y - 7},
                           {center.x + facing * 12, center.y - 14}, 2, Pink);
            }
        }
    for (const auto& shot : game.Shots())
    {
        DrawRectangleRec({shot.position.x - 3, shot.position.y - 2, 10, 22}, Fade(Cyan, 0.15F));
        DrawRectangleRec({shot.position.x, shot.position.y, 4, 14}, Cyan);
        DrawRectangleRec({shot.position.x + 1, shot.position.y, 2, 10}, WHITE);
    }
    if (game.GetSpider()) SpiderArt(*game.GetSpider(), time);
    if (game.GetPlayer().lives > 0 && (game.GetPlayer().invulnerability <= 0.0F ||
        static_cast<int>(time * 10.0F) % 2 == 0))
        PlayerArt(game.GetPlayer().position, Cyan);
}

void Renderer::Hud(const GameModel& game) const
{
    DrawRectangle(0, 0, static_cast<int>(Width), 88, Panel);
    DrawText("CENTIPEDE", 26, 20, 30, Lime);
    DrawText("PRACTICE / SIYATHWALA", 28, 56, 12, Muted);
    DrawText("SCORE", 365, 17, 12, Muted);
    DrawText(TextFormat("%06i", game.Score()), 365, 37, 29, WHITE);
    DrawText("BEST", 575, 17, 12, Muted);
    DrawText(TextFormat("%06i", std::max(game.Score(), game.Scores().Best())), 575, 37, 29, Lime);
    DrawText("LIVES", 845, 17, 12, Muted);
    for (int life = 0; life < game.GetPlayer().lives; ++life)
    {
        const float x = 855.0F + life * 30.0F;
        DrawTriangle({x, 39}, {x - 9, 55}, {x + 9, 55}, Cyan);
    }
    DrawText("P  PAUSE", 1040, 25, 15, Muted);
    DrawText("H  HIGH SCORES", 1040, 49, 12, Muted);
    DrawRectangle(0, 87, static_cast<int>(Width), 1, Fade(Lime, 0.4F));
    DrawRectangle(0, 780, static_cast<int>(Width), 20, Ink);
    DrawText("ARROWS / WASD  MOVE     SPACE  FIRE     ESC  QUIT", 26, 784, 11, Muted);
    DrawText("12 SEGMENTS  /  3 LASERS  /  3 LIVES", 875, 784, 11, Muted);
}

void Renderer::Overlay(const GameModel& game, bool showScores) const
{
    if (game.GetState() == State::Playing && !showScores) return;
    DrawRectangle(0, 88, 1200, 692, Fade(Ink, 0.78F));
    DrawRectangleRounded({280, 150, 640, 520}, 0.035F, 8, Panel);
    DrawRectangleRoundedLinesEx({280, 150, 640, 520}, 0.035F, 8, 1, Fade(Cyan, 0.35F));
    if (showScores)
    {
        CenterText("HIGH SCORES", 195, 42, Lime);
        CenterText("Saved between games", 250, 18, Muted);
        const auto& scores = game.Scores().Entries();
        if (scores.empty()) CenterText("Finish a game to set your first score.", 365, 20, WHITE);
        for (std::size_t index = 0; index < scores.size(); ++index)
        {
            const auto y = 305 + static_cast<int>(index) * 45;
            DrawText(TextFormat("%02i", static_cast<int>(index + 1)), 405, y, 25, Cyan);
            DrawText(TextFormat("%06i", scores[index]), 635, y, 25, WHITE);
        }
        CenterText("H  BACK", 585, 20, Cyan);
        return;
    }
    const auto state = game.GetState();
    if (state == State::Menu)
    {
        CenterText("CENTIPEDE", 205, 58, Lime);
        CenterText("Clear the field. Survive the swarm.", 278, 20, WHITE);
        CenterText("ARROWS / WASD    Move in the lower field", 335, 19, Muted);
        CenterText("SPACE    Fire up to three lasers", 376, 19, Muted);
        CenterText("P    Pause        H    Saved high scores", 417, 19, Muted);
        CenterText("R    Restart when paused or after a game", 458, 19, Muted);
        CenterText("ESC    Quit", 499, 19, Muted);
        CenterText("ENTER  START GAME", 577, 27, Cyan);
    }
    else if (state == State::Paused)
    {
        CenterText("PAUSED", 255, 52, Lime);
        CenterText("P  RESUME", 365, 25, Cyan);
        CenterText("R  RESTART       H  HIGH SCORES", 430, 19, Muted);
        CenterText("ESC  QUIT", 510, 19, Muted);
    }
    else
    {
        CenterText(state == State::Won ? "FIELD CLEARED" : "GAME OVER", 235, 48,
                   state == State::Won ? Lime : Pink);
        CenterText("SCORE  " + std::to_string(game.Score()), 333, 32, WHITE);
        CenterText(game.SaveFailed() ? "Score could not be saved to disk." :
                   "Your high scores are saved between games.", 402, 18,
                   game.SaveFailed() ? Pink : Muted);
        CenterText("ENTER / R  PLAY AGAIN", 503, 25, Cyan);
        CenterText("H  HIGH SCORES       ESC  QUIT", 565, 18, Muted);
    }
}

void Renderer::Draw(const GameModel& game, float time, bool showScores) const
{
    Background(time);
    World(game, time);
    Hud(game);
    Overlay(game, showScores);
}
