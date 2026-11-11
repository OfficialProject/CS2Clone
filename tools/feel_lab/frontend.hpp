#pragma once

#include "bomb_round.hpp"
#include "settings.hpp"
#include "ui.hpp"

namespace feel_lab {

struct Frontend {
    enum class Screen { Home, Lobby, Match, Pause, Result };
    enum class RoundCommand { Continue, Restart, Settings, Lobby };
    Screen screen = Screen::Home;
    GameMode selected = GameMode::Practice;

    enum class Action { None, Start, Resume, Exit };

    bool menu() const { return screen != Screen::Match; }

    const char* mode_name() const {
        switch (selected) {
        case GameMode::Practice: return "Target Practice";
        case GameMode::OfflineMatch: return "Offline Match";
        case GameMode::Plant: return "Plant the Bomb";
        case GameMode::Defuse: return "Defuse the Bomb";
        }
        return "";
    }

    const char* mode_description() const {
        switch (selected) {
        case GameMode::Practice: return "Clear five targets against the clock.";
        case GameMode::OfflineMatch: return "Bomb rounds against two bots.";
        case GameMode::Plant: return "Reach A or B and plant the C4.";
        case GameMode::Defuse: return "Reach A and defuse before the fuse ends.";
        }
        return "";
    }

    Action draw(Settings& settings, bool mirage, bool won, const char* result, const char* next_label="Play again", const char* score="") {
        if (!menu() || settings.visible) return Action::None;
        const int width=GetScreenWidth();
        const int height=GetScreenHeight();
        const auto current=screen;
        const bool selection=current==Screen::Lobby;
        if (!selection) DrawRectangle(0,64,width,height-64,{8,13,18,95});
        ui::bar();
        if (current==Screen::Home || current==Screen::Lobby) {
            if (ui::button(168,13,82,38,"Home",current==Screen::Home)) screen=Screen::Home;
            if (ui::button(258,13,82,38,"Play",selection)) screen=Screen::Lobby;
            if (ui::button(348,13,92,38,"Settings")) settings.visible=true;
            if (ui::button(width-75,13,59,38,"Quit")) return Action::Exit;
        }

        if (current==Screen::Home) {
            const int left=std::max(24,width/18);
            const int top=height/2-98;
            DrawRectangleGradientH(0,64,width,height-64,{6,11,16,230},{6,11,16,10});
            ui::label("CS2CLONE",left,top,64);
            ui::label("Mirage practice",left,top+83,25,ui::muted);
            if (ui::button(left,top+138,180,46,"Open lobby",false,true)) screen=Screen::Lobby;
        } else if (selection) {
            const int margin=std::clamp(width/22,18,64);
            const int content=width-2*margin;
            const int map_width=content*58/100;
            const int details_x=margin+map_width+14;
            const int details_width=width-margin-details_x;
            const int card_top=202;
            const int card_height=height-card_top-92;
            constexpr Color backdrop{8,13,18,210};
            DrawRectangle(0,64,width,card_top-64,backdrop);
            DrawRectangle(0,card_top,margin,card_height,backdrop);
            DrawRectangle(margin+map_width,card_top,width-margin-map_width,card_height,backdrop);
            DrawRectangle(0,card_top+card_height,width,height-card_top-card_height,backdrop);
            ui::label("LOCAL LOBBY",margin,82,29);
            const int tab_width=(content-24)/4;
            using ModeButton = ui::MenuButton<GameMode>;
            const std::array mode_buttons{
                ModeButton{"Targets", GameMode::Practice, selected==GameMode::Practice},
                ModeButton{"Plant", GameMode::Plant, selected==GameMode::Plant, false, mirage},
                ModeButton{"Defuse", GameMode::Defuse, selected==GameMode::Defuse, false, mirage},
                ModeButton{"Offline Match", GameMode::OfflineMatch, selected==GameMode::OfflineMatch, false, mirage}
            };
            if (const auto mode = ui::menu_buttons({margin,132,tab_width,40,tab_width+8,0}, mode_buttons))
                selected = *mode;

            DrawRectangle(margin,card_top,map_width,card_height,{15,25,33,70});
            DrawRectangleGradientV(margin,card_top+card_height-102,map_width,102,{10,17,24,25},{10,17,24,245});
            DrawRectangleLinesEx({static_cast<float>(margin),static_cast<float>(card_top),
                                  static_cast<float>(map_width),static_cast<float>(card_height)},2,ui::accent);
            ui::label(mirage?"MIRAGE":"TARGET RANGE",margin+18,card_top+card_height-57,34);

            DrawRectangle(details_x,card_top,details_width,card_height,ui::panel);
            ui::label("SESSION",details_x+16,card_top+20,15,ui::muted);
            ui::label(selected==GameMode::OfflineMatch?"Local / 1 player / 2 bots":"Local / 1 player",details_x+16,card_top+49,19);
            DrawRectangle(details_x+16,card_top+87,details_width-32,1,ui::line);
            ui::label(mode_name(),details_x+16,card_top+106,22);
            if (details_width>=340) ui::label(mode_description(),details_x+16,card_top+147,16,ui::muted);
            DrawRectangle(0,height-72,width,72,{17,23,29,255});
            ui::label(mirage?"Mirage":"Target Range",margin,height-49,19,ui::muted);
            if (ui::button(width-margin-146,height-58,146,44,"GO",false,true)) return Action::Start;
        } else {
            const int left=(width-428)/2;
            const int top=(height-340)/2;
            DrawRectangle(left,top,428,340,ui::panel);
            DrawRectangle(left,top,428,2,ui::accent);
            const bool paused=current==Screen::Pause;
            ui::label(paused?"PAUSED":won?"ROUND WON":"ROUND LOST",left+24,top+25,34,
                      paused?ui::text:won?ui::accent:ORANGE);
            ui::label(paused?mode_name():result,left+24,top+77,18,ui::muted);
            if (!paused) ui::label(score,left+24,top+105,17,ui::accent);
            using RoundButton = ui::MenuButton<RoundCommand>;
            const std::array round_buttons{
                RoundButton{paused?"Resume":next_label, RoundCommand::Continue, false, true},
                RoundButton{"Restart round", RoundCommand::Restart, false, false, paused},
                RoundButton{"Settings", RoundCommand::Settings},
                RoundButton{"Return to lobby", RoundCommand::Lobby}
            };
            if (const auto command = ui::menu_buttons({left+24,top+137,380,41,0,45}, round_buttons)) {
                switch (*command) {
                case RoundCommand::Continue: return paused?Action::Resume:Action::Start;
                case RoundCommand::Restart: return Action::Start;
                case RoundCommand::Settings: settings.visible=true; break;
                case RoundCommand::Lobby: screen=Screen::Lobby; break;
                }
            }
        }
        return Action::None;
    }
};

} // namespace feel_lab
