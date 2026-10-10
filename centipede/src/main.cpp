#include "GameModel.h"
#include "Renderer.h"
#include <raylib.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1200, 800, "Centipede Practice - SiyaThwala");
    if (!IsWindowReady())
    {
        std::cerr << "Unable to open the game window. Check your graphics driver.\n";
        return 1;
    }
    SetTargetFPS(60);
    {
        using namespace centipede;
        // Resolve saved data beside the executable, independent of launch folder.
        const auto file = std::filesystem::path{GetApplicationDirectory()} /
                          "data" / "high-scores.txt";
        GameModel game{file};
        Renderer renderer;
        bool showScores = false, resumeAfterScores = false;
        float accumulator = 0.0F;
        constexpr float step = 1.0F / 120.0F;
        while (!WindowShouldClose())
        {
            if (IsKeyPressed(KEY_H))
            {
                showScores = !showScores;
                if (showScores)
                {
                    resumeAfterScores = game.GetState() == State::Playing;
                    if (resumeAfterScores) game.TogglePause();
                }
                else if (resumeAfterScores) game.TogglePause();
            }
            if (!showScores)
            {
                if (IsKeyPressed(KEY_P)) game.TogglePause();
                const auto state = game.GetState();
                if ((IsKeyPressed(KEY_ENTER) && (state == State::Menu || state == State::Won || state == State::Lost)) ||
                    (IsKeyPressed(KEY_R) && (state == State::Paused || state == State::Won || state == State::Lost)))
                    game.Start();
            }
            if (game.GetState() == State::Playing && !showScores)
            {
                const Input input{
                    static_cast<float>((IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) -
                                       (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))),
                    static_cast<float>((IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) -
                                       (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))), IsKeyDown(KEY_SPACE)};
                accumulator += std::min(GetFrameTime(), 0.1F);
                while (accumulator >= step)
                { game.Update(step, input); accumulator -= step; }
            }
            else accumulator = 0.0F;
            BeginDrawing();
            renderer.Draw(game, static_cast<float>(GetTime()), showScores);
            EndDrawing();
        }
    }
    CloseWindow();
    return 0;
}
