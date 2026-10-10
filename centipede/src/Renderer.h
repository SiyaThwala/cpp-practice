#pragma once
#include "GameModel.h"

/** Draws the game using raylib shapes; no image assets are required. */
class Renderer
{
public:
    void Draw(const centipede::GameModel& game, float time, bool showScores) const;

private:
    void Background(float time) const;
    void World(const centipede::GameModel& game, float time) const;
    void Hud(const centipede::GameModel& game) const;
    void Overlay(const centipede::GameModel& game, bool showScores) const;
};
