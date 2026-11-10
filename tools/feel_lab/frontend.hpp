#pragma once

#include "bomb_round.hpp"
#include "settings.hpp"

namespace feel_lab {

struct Frontend {
    enum class Screen { Home, Lobby, Match, Pause, Result };
    Screen screen = Screen::Home;
    GameMode selected = GameMode::Practice;

    enum class Action { None, Start, Resume, Exit };

    bool menu() const { return screen != Screen::Match; }

    Action draw(Settings& settings, bool mirage, bool won, const char* result) {
        if (!menu() || settings.visible) return Action::None;
        const int x = (GetScreenWidth()-480)/2;
        const int y = (GetScreenHeight()-450)/2;
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{6,11,18,208});
        DrawRectangle(x,y,480,450,{17,25,37,255});
        DrawRectangle(x,y,480,3,{68,175,159,255});
        const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const auto button = [&](int row, const char* label) {
            const Rectangle bounds{static_cast<float>(x+28),static_cast<float>(y+row),424,39};
            Settings::draw_button(bounds,label);
            return clicked && CheckCollisionPointRec(GetMousePosition(),bounds);
        };
        if (screen == Screen::Home) {
            DrawText("CS2CLONE",x+28,y+38,42,RAYWHITE);
            DrawText("Movement and bomb practice",x+30,y+96,19,LIGHTGRAY);
            if (button(158,"Open lobby")) screen = Screen::Lobby;
            if (button(210,"Settings")) settings.visible = true;
            if (button(262,"Quit")) return Action::Exit;
        } else if (screen == Screen::Lobby) {
            DrawText("LOCAL LOBBY",x+28,y+24,30,RAYWHITE);
            DrawText(mirage ? "MIRAGE   /   1 PLAYER" : "TARGET RANGE   /   1 PLAYER",x+28,y+68,17,LIGHTGRAY);
            if (button(106,selected == GameMode::Practice ? "> Target practice" : "Target practice"))
                selected = GameMode::Practice;
            if (mirage && button(153,selected == GameMode::Plant ? "> Plant the bomb   /   T" : "Plant the bomb   /   T"))
                selected = GameMode::Plant;
            if (mirage && button(200,selected == GameMode::Defuse ? "> Defuse the bomb   /   CT" : "Defuse the bomb   /   CT"))
                selected = GameMode::Defuse;
            if (button(260,"Start round")) return Action::Start;
            if (button(307,"Settings")) settings.visible = true;
            if (button(354,"Back")) screen = Screen::Home;
        } else if (screen == Screen::Pause) {
            DrawText("PAUSED",x+28,y+32,34,RAYWHITE);
            if (button(105,"Resume")) return Action::Resume;
            if (button(154,"Restart round")) return Action::Start;
            if (button(203,"Settings")) settings.visible = true;
            if (button(252,"Return to lobby")) screen = Screen::Lobby;
            if (button(301,"Quit")) return Action::Exit;
        } else {
            DrawText(won ? "ROUND WON" : "ROUND LOST",x+28,y+35,36,won?GREEN:ORANGE);
            DrawText(result,x+28,y+94,19,LIGHTGRAY);
            if (button(158,"Play again")) return Action::Start;
            if (button(210,"Return to lobby")) screen = Screen::Lobby;
            if (button(262,"Settings")) settings.visible = true;
        }
        return Action::None;
    }
};

} // namespace feel_lab
