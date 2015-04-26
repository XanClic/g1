#ifndef SOFTWARE_HPP
#define SOFTWARE_HPP

#include <string>

extern "C" {
#include <lua.h>
}

#include "physics.hpp"
#include "ship.hpp"
#include "ui.hpp"


class SpecializedSoftware;


class Software {
    public:
        enum Type {
            FLIGHT_CONTROL,
            SCENARIO,

            TYPE_MAX
        };

    private:
        lua_State *ls;
        Type t;
        std::string enm, nm;
        SpecializedSoftware *sw;

        void sg(const char *name, int value);

    public:
        Software(const std::string &name, const std::string &filename);

        Type type(void) const { return t; }
        const std::string &name(void) const { return nm; }

        template<typename T> T &sub(void);

        friend class SpecializedSoftware;
};


class SpecializedSoftware {
    protected:
        Software *base;

        SpecializedSoftware(Software *sw):
            base(sw)
        {}

        lua_State *ls(void) { return base->ls; }
        const std::string &enm(void) { return base->enm; }
};


class FlightControlSoftware: public SpecializedSoftware {
    private:
        FlightControlSoftware(Software *sw):
            SpecializedSoftware(sw)
        {}

    public:
        void execute(ShipState &ship, const Input &input);

        friend class Software;
};

template<> FlightControlSoftware &Software::sub<FlightControlSoftware>(void);


class ScenarioScript: public SpecializedSoftware {
    private:
        ScenarioScript(Software *sw):
            SpecializedSoftware(sw)
        {}

        WorldState *current_world_state;

        static int luaw_enable_player_physics(lua_State *ls);
        static int luaw_fix_player_to_ground(lua_State *ls);
        static int luaw_set_player_position(lua_State *ls);
        static int luaw_set_player_velocity(lua_State *ls);
        static int luaw_set_player_bearing(lua_State *ls);

    public:
        void execute(WorldState &out_world, const WorldState &in_world, const Input &input);
        void initialize(WorldState &world);

        friend class Software;
};

template<> ScenarioScript &Software::sub<ScenarioScript>(void);


void load_software(void);

void execute_flight_control_software(ShipState &ship, const Input &input);

Software *get_scenario(const std::string &name);

#endif
