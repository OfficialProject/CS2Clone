#pragma once

#include "imported_map.hpp"
#include "gunplay.hpp"
#include <array>

namespace feel_lab {

enum class Weapon { Knife, Pistol, Rifle };

struct WeaponInfo {
    const char* name;
    int magazine;
    int damage;
    double interval;
    double reload;
    float reach;
};

inline constexpr std::array<WeaponInfo,3> weapons{{
    {"Knife",0,50,0.55,0,1.65f},
    {"Pistol",20,30,0.15,2.2,100},
    {"Rifle",30,36,0.10,2.5,100}
}};

struct Opponent {
    Vector3 feet{};
    int health=100;
    double shot_timer=0;
    Vector3 guard{};
    bool alive() const { return health>0; }
    BoundingBox hitbox() const {
        return {{feet.x-0.28f,feet.y,feet.z-0.28f},{feet.x+0.28f,feet.y+1.65f,feet.z+0.28f}};
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
    std::array<Gunplay,3> gunplay{};
    Weapon equipped=Weapon::Pistol;
    double cooldown=0, reload=0;
    int won_rounds=0, lost_rounds=0, completed=0;
    int kills=0, health=100, armor=0, cash=800;
    int deaths=0;
    float shot_kick=0;
    bool rifle=false, kit=false, buying=false, won_round=false;
    const char* result="";
    const char* purchase_message="";

    bool attacking() const { return completed<3; }
    bool finished() const { return won_rounds>=4 || lost_rounds>=4 || completed>=6; }
    const WeaponInfo& weapon() const { return weapons[static_cast<std::size_t>(equipped)]; }
    int& magazine() { return ammo[static_cast<std::size_t>(equipped)]; }
    bool bots_alive() const { return bots[0].alive() || bots[1].alive(); }

    void advance_gunplay(double dt, bool landed) {
        gunplay[1].advance(pistol_accuracy,weapons[1].interval,dt,landed);
        gunplay[2].advance(rifle_accuracy,weapons[2].interval,dt,landed);
    }

    float spread(const game::simulation::MovementState& state) const {
        if (equipped==Weapon::Knife) return 0;
        return gunplay[static_cast<std::size_t>(equipped)].spread(
            equipped==Weapon::Pistol?pistol_accuracy:rifle_accuracy,state);
    }

    void start(const ImportedMap& map, bool new_match) {
        if (new_match) *this=OfflineMatch{};
        if (completed==3) cash=800;
        phase=Phase::Buy;
        time=8;
        interaction=elapsed=cooldown=reload=0;
        shot_kick=0;
        gunplay={};
        health=100;
        armor=0;
        rifle=kit=false;
        buying=true;
        purchase_message="";
        equipped=Weapon::Pistol;
        ammo={0,weapons[1].magazine,weapons[2].magazine};
        const auto site=map.spawn_player({-49.88f,-3.24f,-10.2f});
        bomb={-49.88f,static_cast<float>(site.vertical_position),-10.2f};
        const std::array<Vector3,2> positions=attacking()
            ? std::array<Vector3,2>{{{-49,-3.2f,-11},{-46,-3.1f,-15.2f}}}
            : std::array<Vector3,2>{{{bomb.x,bomb.y+0.1f,bomb.z},{-50.7f,-3.2f,-12.2f}}};
        for (std::size_t i=0;i<bots.size();++i) {
            const auto point=map.spawn_player(positions[i]);
            const Vector3 feet{positions[i].x,static_cast<float>(point.vertical_position),positions[i].z};
            bots[i]={feet,100,0.7+static_cast<double>(i)*0.25,feet};
        }
    }

    void purchase(int item) {
        const int price=item==0?2700:item==1?650:400;
        const bool owned=item==0?rifle:item==1?armor>0:kit;
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

    void select(Weapon weapon_choice) {
        if (weapon_choice==Weapon::Rifle && !rifle) return;
        if (equipped!=weapon_choice) {
            equipped=weapon_choice;
            reload=0;
            cooldown=std::max(cooldown,0.18);
        }
    }

    void finish(bool win,const char* reason) {
        if (phase==Phase::Result) return;
        phase=Phase::Result;
        buying=false;
        won_round=win;
        result=reason;
        ++completed;
        if (win) ++won_rounds;
        else ++lost_rounds;
        cash=std::min(16000,cash+(win?3250:1900));
    }

    void step(double dt, const ImportedMap& map, Vector3 feet, Vector3 eye,
              bool on_site, bool grounded, bool use) {
        if (phase==Phase::Buy) {
            time-=dt;
            if (time<=0) {
                phase=Phase::Live;
                buying=false;
                time=115;
            }
            return;
        }
        if (phase==Phase::Result) return;
        time-=dt;
        elapsed+=dt;
        cooldown=std::max(0.0,cooldown-dt);
        shot_kick=std::max(0.0f,shot_kick-static_cast<float>(dt)*4);
        if (reload>0) {
            reload=std::max(0.0,reload-dt);
            if (reload==0) magazine()=weapon().magazine;
        }

        for (auto& bot : bots) {
            if (!bot.alive()) continue;
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
            if (bot.shot_timer>0) continue;
            const auto origin=Vector3Add(bot.feet,{0,1.46f,0});
            const auto delta=Vector3Subtract(eye,origin);
            const float distance=Vector3Length(delta);
            if (distance>0.01f && distance<25 &&
                map.collision.shot_distance({origin,Vector3Scale(delta,1/distance)},distance)>=distance-0.03f) {
                const int damage=armor>0?8:14;
                armor=std::max(0,armor-8);
                health=std::max(0,health-damage);
                bot.shot_timer=0.65;
            } else bot.shot_timer=0.2;
        }
        if (health==0) {
            ++deaths;
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
            if (interaction>=3 && time>=0) {
                if (attacking()) bomb=feet;
                phase=Phase::Planted;
                time=40;
                interaction=0;
            } else if (time<=0) finish(!attacking(),"Round time expired");
            return;
        }

        if (!attacking()) {
            const bool defusing=grounded && use && Vector3Distance(feet,bomb)<1.65f;
            interaction=defusing?interaction+dt:0;
            if (interaction>=(kit?5:10) && time>=0) {
                finish(true,"Bomb defused");
                return;
            }
        } else {
            bool defender_near=false;
            for (const auto& bot : bots)
                defender_near=defender_near || (bot.alive() && Vector3Distance(bot.feet,bomb)<2.0f);
            interaction=defender_near?interaction+dt:0;
            if (interaction>=10 && time>=0) {
                finish(false,"Defender defused the bomb");
                return;
            }
        }
        if (time<=0) finish(attacking(),"Bomb detonated");
    }

    bool fire(const ImportedMap& map, Ray aim, const game::simulation::MovementState& state) {
        if (phase==Phase::Buy || phase==Phase::Result || cooldown>0 || reload>0 ||
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
        for (auto& bot : bots) {
            if (!bot.alive()) continue;
            const auto hit=GetRayCollisionBox(aim,bot.hitbox());
            if (hit.hit && hit.distance<nearest) {
                target=&bot;
                nearest=hit.distance;
            }
        }
        if (!target) return false;
        target->health=std::max(0,target->health-weapon().damage);
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
            DrawCube(Vector3Add(bot.feet,{0,0.66f,0}),0.56f,1.32f,0.56f,body);
            DrawSphere(Vector3Add(bot.feet,{0,1.48f,0}),0.17f,body);
        }
    }
};

} // namespace feel_lab
