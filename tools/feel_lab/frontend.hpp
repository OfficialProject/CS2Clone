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
    MatchLength match_length = MatchLength::Short;

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

    Action draw(Settings& settings, bool defusal, bool won, const char* result,
                const char* next_label="Play again", const char* score="", const char* map_name="Mirage",
                bool final_score=false) {
        if (!menu() || settings.visible) return Action::None;
        const int width=GetScreenWidth();
        const int height=GetScreenHeight();
        const auto current=screen;
        const bool selection=current==Screen::Lobby;
        if (!selection) DrawRectangle(0,64,width,height-64,{9,16,19,98});
        ui::bar();
        if (current==Screen::Home || current==Screen::Lobby) {
            if (ui::button(168,13,82,38,"Home",current==Screen::Home)) screen=Screen::Home;
            if (ui::button(258,13,82,38,"Play",selection)) screen=Screen::Lobby;
            if (ui::button(348,13,92,38,"Settings")) settings.visible=true;
            if (ui::button(width-75,13,59,38,"Quit")) return Action::Exit;
        }

        if (current==Screen::Home) {
            const int left=std::clamp(width/13,36,110);
            const int top=height/2-70;
            DrawRectangleGradientH(0,64,width,height-64,{8,14,17,250},{8,14,17,0});
            DrawRectangleGradientV(0,height-155,width,155,{8,14,17,0},{8,14,17,220});
            DrawRectangle(left,top,43,3,ui::accent);
            ui::heading(map_name,left,top+20,std::clamp(width/16,34,74));
            ui::label(defusal?"Bomb defusal and target practice":"Target practice",left,top+105,21,ui::muted);
            if (ui::button(left,top+153,196,46,"Play",false,true)) screen=Screen::Lobby;
        } else if (selection) {
            const int margin=std::clamp(width/25,18,60);
            const int rail=std::clamp(width/4,166,300);
            const int card_x=margin+rail+14;
            const int card_width=width-margin-card_x;
            const int card_top=142;
            const int card_bottom=height-96;
            DrawRectangle(0,64,width,height-64,{9,16,19,145});
            DrawRectangleGradientH(0,64,width,height-64,{9,16,19,135},{9,16,19,0});
            ui::heading("PLAY",margin,82,34);
            ui::surface(margin,card_top,rail,card_bottom-card_top,{22,28,30,244});
            ui::label("LOCAL MODES",margin+14,card_top+13,16,ui::muted);
            using ModeButton = ui::MenuButton<GameMode>;
            const std::array mode_buttons{
                ModeButton{"Target practice", GameMode::Practice, selected==GameMode::Practice},
                ModeButton{"Offline match", GameMode::OfflineMatch, selected==GameMode::OfflineMatch, false, defusal},
                ModeButton{"Plant drill", GameMode::Plant, selected==GameMode::Plant, false, defusal},
                ModeButton{"Defuse drill", GameMode::Defuse, selected==GameMode::Defuse, false, defusal}
            };
            if (const auto mode = ui::menu_buttons({margin+12,card_top+45,rail-24,47,0,54}, mode_buttons))
                selected = *mode;
            if (selected==GameMode::OfflineMatch &&
                ui::button(margin+12,card_bottom-55,rail-24,42,
                           match_length==MatchLength::Short?"Short / 6 rounds":"Classic / 30 rounds")) {
                match_length=match_length==MatchLength::Short?MatchLength::Classic:MatchLength::Short;
            }

            ui::surface(card_x,card_top,card_width,card_bottom-card_top,{13,21,24,16});
            DrawRectangleGradientV(card_x,card_bottom-155,card_width,155,{13,21,24,0},{13,21,24,255});
            DrawRectangle(card_x,card_bottom-2,card_width,2,ui::accent);
            ui::heading(map_name,card_x+18,card_bottom-116,std::clamp(card_width/11,24,42));
            ui::label(mode_name(),card_x+18,card_bottom-66,19);
            int description_size=16;
            while (description_size>12 && ui::label_width(mode_description(),description_size)>card_width-36)
                --description_size;
            ui::label(mode_description(),card_x+18,card_bottom-36,description_size,ui::muted);
            ui::surface(0,height-76,width,76,{20,26,29,248});
            ui::label(selected==GameMode::OfflineMatch?"1 player / 2 bots":"1 player",margin,height-48,17,ui::muted);
            if (ui::button(width-margin-170,height-61,170,46,"Start session",false,true)) return Action::Start;
        } else {
            const int left=(width-428)/2;
            const int top=(height-340)/2;
            ui::surface(left,top,428,340);
            DrawRectangle(left,top,428,2,ui::accent);
            const bool paused=current==Screen::Pause;
            const char* title=paused?"PAUSED":final_score?"MATCH RESULT":won?"ROUND WON":"ROUND LOST";
            ui::label(title,left+24,top+25,34,paused||final_score?ui::text:won?ui::accent:ORANGE);
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
