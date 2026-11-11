#pragma once

#include "bomb_round.hpp"
#include "freight_yard.hpp"
#include "gunplay.hpp"
#include "utility.hpp"
#include <array>

namespace feel_lab {

enum class Weapon { Knife, Pistol, Rifle };

struct WeaponInfo {
    const char* name;
    int magazine;
    int reserve;
    int damage;
    double interval;
    double reload;
    float reach;
    float range_modifier;
};

inline constexpr std::array<WeaponInfo,3> weapons{{
    {"Knife",0,0,50,0.55,0,1.65f,1.0f},
    {"Pistol",20,120,30,0.15,2.2,100,0.91f},
    {"Rifle",30,90,36,0.10,2.5,100,0.98f}
}};

struct Opponent {
    static constexpr float half_width=0.28f;
    static constexpr float body_height=1.32f;
    static constexpr float head_radius=0.17f;
    Vector3 feet{};
    int health=100;
    double shot_timer=0;
    Vector3 guard{};
    double blinded=0;
    bool fired=false;
    bool alive() const { return health>0; }
    Vector3 head() const { return Vector3Add(feet,{0,1.48f,0}); }
    BoundingBox body_hitbox() const {
        return {{feet.x-half_width,feet.y,feet.z-half_width},
                {feet.x+half_width,feet.y+body_height,feet.z+half_width}};
    }
};

struct OfflineMatch {
    enum class Phase { Buy, Live, Planted, Result };
    Phase phase=Phase::Buy;
    double time=8;
    double interaction=0;
    double elapsed=0;
    Vector3 bomb{};
    std::array<Opponent,2> bots{};
    std::array<int,3> ammo{};
    std::array<int,3> reserve{};
    std::array<Gunplay,3> gunplay{};
    Utility utility;
    Weapon equipped=Weapon::Pistol;
    double cooldown=0, reload=0;
    int won_rounds=0, lost_rounds=0, completed=0;
    int rounds_per_half=3;
    int kills=0, health=100, armor=0, cash=800;
    int deaths=0;
    int consecutive_losses=0;
    float shot_kick=0;
    bool rifle=false, kit=false, buying=false, won_round=false;
    const char* result="";
    const char* purchase_message="";
    struct RoundLoadout {
        int cash=800, armor=0, kills=0, deaths=0;
        bool rifle=false, kit=false;
        std::array<bool,2> grenades{};
    } round_start;

    bool attacking() const { return completed<rounds_per_half; }
    bool finished() const { return won_rounds>rounds_per_half || lost_rounds>rounds_per_half || completed>=2*rounds_per_half; }
    const WeaponInfo& weapon() const { return weapons[static_cast<std::size_t>(equipped)]; }
    int& magazine() { return ammo[static_cast<std::size_t>(equipped)]; }
    int& spare_ammo() { return reserve[static_cast<std::size_t>(equipped)]; }
    bool alive() const { return health>0; }
    bool bots_alive() const { return bots[0].alive() || bots[1].alive(); }
    double defuse_duration() const { return kit?BombRound::defuse_seconds/2:BombRound::defuse_seconds; }

    void advance_gunplay(double dt, bool landed) {
        gunplay[1].advance(pistol_accuracy,weapons[1].interval,dt,landed);
        gunplay[2].advance(rifle_accuracy,weapons[2].interval,dt,landed);
    }

    float spread(const game::simulation::MovementState& state) const {
        if (equipped==Weapon::Knife) return 0;
        return gunplay[static_cast<std::size_t>(equipped)].spread(
            equipped==Weapon::Pistol?pistol_accuracy:rifle_accuracy,state);
    }

    void start(const ImportedMap& map, bool new_match, MatchLength length=MatchLength::Short) {
        if (new_match) {
            *this=OfflineMatch{};
            rounds_per_half=length==MatchLength::Classic?15:3;
        } else if (phase!=Phase::Result) {
            cash=round_start.cash;
            armor=round_start.armor;
            kills=round_start.kills;
            deaths=round_start.deaths;
            rifle=round_start.rifle;
            kit=round_start.kit;
            utility.owned=round_start.grenades;
        } else {
            if (!alive() || completed==rounds_per_half) {
                armor=0;
                rifle=kit=false;
                utility.owned={};
            }
            if (completed==rounds_per_half) {
                cash=800;
                consecutive_losses=0;
            }
        }
        round_start={cash,armor,kills,deaths,rifle,kit,utility.owned};
        utility={};
        utility.owned=round_start.grenades;
        phase=Phase::Buy;
        time=8;
        interaction=elapsed=cooldown=reload=0;
        shot_kick=0;
        gunplay={};
        health=100;
        buying=true;
        purchase_message="";
        equipped=rifle?Weapon::Rifle:Weapon::Pistol;
        ammo={0,weapons[1].magazine,weapons[2].magazine};
        reserve={0,weapons[1].reserve,weapons[2].reserve};
        const auto site_origin=map.original ? Vector3{-16,0.3f,-7} : Vector3{-49.88f,-3.24f,-10.2f};
        const auto site=map.spawn_player(site_origin);
        bomb={site_origin.x,static_cast<float>(site.vertical_position),site_origin.z};
        std::array<Vector3,2> positions;
        if (map.original) {
            positions=attacking() ? std::array<Vector3,2>{{{-17,0.3f,-8},{16,0.3f,-8}}}
                                  : std::array<Vector3,2>{{{-16,0.3f,-6},{-19,0.3f,-5}}};
        } else {
            positions=attacking() ? std::array<Vector3,2>{{{-49,-3.2f,-11},{-46,-3.1f,-15.2f}}}
                                  : std::array<Vector3,2>{{{bomb.x,bomb.y+0.1f,bomb.z},{-50.7f,-3.2f,-12.2f}}};
        }
        for (std::size_t i=0;i<bots.size();++i) {
            const auto point=map.spawn_player(positions[i]);
            const Vector3 feet{positions[i].x,static_cast<float>(point.vertical_position),positions[i].z};
            bots[i]={feet,100,0.7+static_cast<double>(i)*0.25,feet};
        }
    }

    void purchase(int item) {
        const int price=item==0?2700:item==1?650:400;
        const bool owned=item==0?rifle:item==1?armor==100:kit;
        if (phase!=Phase::Buy || owned || (item==2 && attacking())) return;
        if (cash<price) {
            purchase_message="Insufficient funds";
            return;
        }
        cash-=price;
        purchase_message="";
        if (item==0) {
            rifle=true;
            select(Weapon::Rifle);
        } else if (item==1) armor=100;
        else kit=true;
    }

    void purchase_grenade(std::size_t index) {
        if (phase!=Phase::Buy || utility.owned[index]) return;
        const int price=index==0?300:200;
        if (cash<price) {
            purchase_message="Insufficient funds";
            return;
        }
        cash-=price;
        purchase_message="";
        utility.owned[index]=true;
    }

    void throw_grenade(std::size_t index, Ray aim) {
        if (!alive() || !utility.owned[index] || (phase!=Phase::Live && phase!=Phase::Planted)) return;
        utility.owned[index]=false;
        utility.grenades[index].launch(aim);
    }

    void step_utility(double dt, const MapCollision& collision, Vector3 eye, Vector3 look) {
        utility.smoke_seconds=std::max(0.0,utility.smoke_seconds-dt);
        utility.player_flash=std::max(0.0,utility.player_flash-dt);
        if (utility.grenades[0].step(collision,dt)) {
            utility.smoke=utility.grenades[0].position;
            utility.smoke_seconds=14;
        }
        if (utility.grenades[1].step(collision,dt)) {
            const auto flash=utility.grenades[1].position;
            if (Utility::flash_visible(collision,flash,eye)) {
                const auto direction=Vector3Normalize(Vector3Subtract(flash,eye));
                const auto facing=std::max(0.0f,Vector3DotProduct(look,direction));
                utility.player_flash=0.25+1.75*facing;
            }
            for (auto& bot : bots)
                if (bot.alive() && Utility::flash_visible(collision,flash,bot.head())) bot.blinded=2.0;
        }
    }

    void select(Weapon weapon_choice) {
        if (weapon_choice==Weapon::Rifle && !rifle) return;
        if (equipped!=weapon_choice) {
            equipped=weapon_choice;
            reload=0;
            cooldown=std::max(cooldown,0.18);
        }
    }

    void finish(bool win,const char* reason,int objective_award=0) {
        if (phase==Phase::Result) return;
        phase=Phase::Result;
        buying=false;
        won_round=win;
        result=reason;
        ++completed;
        if (win) ++won_rounds;
        else ++lost_rounds;
        consecutive_losses=win?0:std::min(consecutive_losses+1,5);
        const int award=win?3250:1400+500*(consecutive_losses-1);
        cash=std::min(16000,cash+award+objective_award);
    }

    void start_reload() {
        if (alive() && reload==0 && magazine()<weapon().magazine && spare_ammo()>0)
            reload=weapon().reload;
    }

    void step(double dt, const ImportedMap& map, Vector3 feet, Vector3 eye,
              bool on_site, bool grounded, bool use, Vector3 look={0,0,-1}) {
        if (phase==Phase::Buy) {
            time-=dt;
            if (time<=0) {
                phase=Phase::Live;
                buying=false;
                time=BombRound::round_seconds;
            }
            return;
        }
        if (phase==Phase::Result) return;
        time-=dt;
        elapsed+=dt;
        step_utility(dt,map.collision,eye,look);
        cooldown=std::max(0.0,cooldown-dt);
        shot_kick=std::max(0.0f,shot_kick-static_cast<float>(dt)*4);
        if (reload>0) {
            reload=std::max(0.0,reload-dt);
            if (reload==0) {
                const int rounds=std::min(spare_ammo(),weapon().magazine-magazine());
                magazine()+=rounds;
                spare_ammo()-=rounds;
            }
        }

        const bool was_alive=alive();
        for (auto& bot : bots) {
            bot.fired=false;
            if (!bot.alive()) continue;
            bot.blinded=std::max(0.0,bot.blinded-dt);
            auto destination=bot.guard;
            if (attacking() && phase==Phase::Planted) {
                destination=bomb;
            } else if (!attacking() && phase==Phase::Live && elapsed>=20) {
                destination=bomb;
            } else if (attacking() || phase==Phase::Planted) {
                destination.x+=static_cast<float>(std::sin(elapsed*1.4))*0.7f;
            }
            Vector3 direction=Vector3Subtract(destination,bot.feet);
            direction.y=0;
            const float distance_to_goal=Vector3Length(direction);
            if (distance_to_goal>0.35f) {
                const auto velocity=Vector3Scale(direction,2.15f/distance_to_goal);
                const auto movement=map.collision.move(bot.feet,velocity,
                    Vector3Scale(velocity,static_cast<float>(dt)),ImportedMap::hull_height(0),true);
                if (movement.grounded) bot.feet=movement.position;
            }
            bot.shot_timer-=dt;
            if (bot.shot_timer>0 || !alive() || bot.blinded>0) continue;
            const auto origin=bot.head();
            const auto delta=Vector3Subtract(eye,origin);
            const float distance=Vector3Length(delta);
            if (distance<=0.01f || distance>=25) {
                bot.shot_timer=0.2;
                continue;
            }
            const Ray sight{origin,Vector3Scale(delta,1/distance)};
            if (!utility.obscures(sight,distance) &&
                map.collision.shot_distance(sight,distance)>=distance-0.03f) {
                const int damage=armor>0?8:14;
                armor=std::max(0,armor-8);
                health=std::max(0,health-damage);
                bot.shot_timer=0.65;
                bot.fired=true;
            } else bot.shot_timer=0.2;
        }
        if (was_alive && !alive()) ++deaths;
        if (!alive() && !(attacking() && phase==Phase::Planted)) {
            finish(false,"Eliminated");
            return;
        }

        if (phase==Phase::Live) {
            if (!bots_alive()) {
                finish(true,"Opposing team eliminated");
                return;
            }
            const bool planting=attacking() && on_site && grounded && use;
            if (attacking()) interaction=planting?interaction+dt:0;
            bool bot_on_site=false;
            for (const auto& bot : bots)
                bot_on_site=bot_on_site || (bot.alive() && Vector3Distance(bot.feet,bomb)<1.65f);
            if (!attacking()) interaction=elapsed>=21 && bot_on_site?interaction+dt:0;
            if (interaction>=BombRound::plant_seconds && time>=0) {
                if (attacking()) {
                    bomb=feet;
                    cash=std::min(16000,cash+300);
                }
                phase=Phase::Planted;
                time=BombRound::fuse_seconds;
                interaction=0;
            } else if (time<=0) finish(!attacking(),"Round time expired");
            return;
        }

        if (!attacking()) {
            const bool defusing=grounded && use && Vector3Distance(feet,bomb)<1.65f;
            interaction=defusing?interaction+dt:0;
            if (interaction>=defuse_duration() && time>=0) {
                finish(true,"Bomb defused",300);
                return;
            }
        } else {
            bool defender_near=false;
            for (const auto& bot : bots)
                defender_near=defender_near || (bot.alive() && Vector3Distance(bot.feet,bomb)<2.0f);
            interaction=defender_near?interaction+dt:0;
            if (interaction>=BombRound::defuse_seconds && time>=0) {
                finish(false,"Defender defused the bomb",800);
                return;
            }
        }
        if (time<=0) finish(attacking(),"Bomb detonated",attacking()?250:0);
    }

    bool fire(const ImportedMap& map, Ray aim, const game::simulation::MovementState& state) {
        if (!alive() || phase==Phase::Buy || phase==Phase::Result || cooldown>0 || reload>0 ||
            (equipped!=Weapon::Knife && magazine()==0)) return false;
        cooldown=weapon().interval;
        if (equipped!=Weapon::Knife) {
            aim=gunplay[static_cast<std::size_t>(equipped)].shoot(
                aim,equipped==Weapon::Pistol?pistol_accuracy:rifle_accuracy,state);
            shot_kick=std::min(1.0f,shot_kick+(equipped==Weapon::Pistol?0.22f:0.32f));
            --magazine();
        }
        float nearest=map.collision.shot_distance(aim,weapon().reach);
        Opponent* target=nullptr;
        bool headshot=false;
        for (auto& bot : bots) {
            if (!bot.alive()) continue;
            const auto body=GetRayCollisionBox(aim,bot.body_hitbox());
            if (body.hit && body.distance<nearest) {
                target=&bot;
                nearest=body.distance;
                headshot=false;
            }
            const auto head=GetRayCollisionSphere(aim,bot.head(),Opponent::head_radius);
            if (head.hit && head.distance<nearest) {
                target=&bot;
                nearest=head.distance;
                headshot=equipped!=Weapon::Knife;
            }
        }
        if (!target) return false;
        constexpr float falloff_distance=500*static_cast<float>(game::simulation::MovementConfig::source_unit);
        const float falloff=std::pow(weapon().range_modifier,nearest/falloff_distance);
        const int damage=static_cast<int>(static_cast<float>(weapon().damage)*falloff*(headshot?4.0f:1.0f));
        target->health=std::max(0,target->health-damage);
        if (!target->alive()) {
            ++kills;
            cash=std::min(16000,cash+300);
        }
        return true;
    }

    void draw_bots() const {
        for (const auto& bot : bots) {
            if (!bot.alive()) continue;
            const Color body=attacking()?Color{64,120,185,255}:Color{176,116,62,255};
            DrawCube(Vector3Add(bot.feet,{0,Opponent::body_height/2,0}),2*Opponent::half_width,
                     Opponent::body_height,2*Opponent::half_width,body);
            DrawSphere(bot.head(),Opponent::head_radius,body);
        }
    }
};

} // namespace feel_lab
