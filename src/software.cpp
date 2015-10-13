#include <dake/dake.hpp>

#include <dirent.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/stat.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "json-structs.hpp"
#include "physics.hpp"
#include "ship.hpp"
#include "ship_types.hpp"
#include "software.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


static const char *const software_type_names[] = {
    "flight control",
    "scenario",
};

static std::vector<Software *> software[Software::TYPE_MAX];


static int luaw_crossp(lua_State *ls);
static int luaw_dotp(lua_State *ls);


Software::Software(const std::string &n, const std::string &filename):
    enm(n)
{
    ls = luaL_newstate();
    luaL_openlibs(ls);

    if (luaL_loadfile(ls, filename.c_str())) {
        throw std::runtime_error("Could not load lua script " + filename + ": " + std::string(lua_tostring(ls, -1)));
    }


    sg("FLIGHT_CONTROL", FLIGHT_CONTROL);
    sg("SCENARIO", SCENARIO);

    sg("THRUSTER_RCS",  THRUSTER_RCS);
    sg("THRUSTER_MAIN", THRUSTER_MAIN);

    sg("RIGHT",    RIGHT);
    sg("LEFT",     LEFT);
    sg("UP",       UP);
    sg("DOWN",     DOWN);
    sg("FORWARD",  FORWARD);
    sg("BACKWARD", BACKWARD);
    sg("TOP",      TOP);
    sg("BOTTOM",   BOTTOM);
    sg("FRONT",    FRONT);
    sg("BACK",     BACK);


    lua_pushcfunction(ls, luaw_crossp);
    lua_setglobal(ls, "crossp");

    lua_pushcfunction(ls, luaw_dotp);
    lua_setglobal(ls, "dotp");


    lua_call(ls, 0, LUA_MULTRET);

    lua_getglobal(ls, "type");
    if (!lua_isnumber(ls, -1)) {
        throw std::runtime_error(enm + ": Invalid type given");
    }

    int it = lua_tointeger(ls, -1);
    if ((it < 0) || (it >= TYPE_MAX)) {
        throw std::runtime_error(enm + ": type out of range");
    }

    t = static_cast<Type>(it);
    lua_pop(ls, 1);

    lua_getglobal(ls, "name");
    if (!lua_isstring(ls, -1)) {
        throw std::runtime_error(enm + ": Invalid name given");
    }

    nm = lua_tolstring(ls, -1, nullptr);
    lua_pop(ls, 1);


    switch (t) {
        case FLIGHT_CONTROL:
            sw = new FlightControlSoftware(this);
            break;

        case SCENARIO:
            sw = new ScenarioScript(this);
            break;

        default:
            throw std::runtime_error(enm + ": Invalid type given");
    }
}


void Software::sg(const char *n, int v)
{
    lua_pushinteger(ls, v);
    lua_setglobal(ls, n);
}


static vec3 lua_tovector(lua_State *ls, int index)
{
    vec3 vec;

    if (lua_isnil(ls, index)) {
        return vec3::zero();
    }

    lua_getfield(ls, index, "x");
    vec.x() = lua_tonumber(ls, -1);
    lua_pop(ls, 1);

    lua_getfield(ls, index, "y");
    vec.y() = lua_tonumber(ls, -1);
    lua_pop(ls, 1);

    lua_getfield(ls, index, "z");
    vec.z() = lua_tonumber(ls, -1);
    lua_pop(ls, 1);

    return vec;
}


static int luaw_veclength(lua_State *ls);
static int luaw_vecrotate(lua_State *ls);
static int luaw_vecadd(lua_State *ls);
static int luaw_vecsub(lua_State *ls);
static int luaw_vecmul(lua_State *ls);
static int luaw_vecdiv(lua_State *ls);


static void lua_pushvector(lua_State *ls, const vec3 &vec)
{
    lua_newtable(ls);

    lua_pushnumber(ls, vec.x());
    lua_setfield(ls, -2, "x");

    lua_pushnumber(ls, vec.y());
    lua_setfield(ls, -2, "y");

    lua_pushnumber(ls, vec.z());
    lua_setfield(ls, -2, "z");

    lua_pushcfunction(ls, luaw_veclength);
    lua_setfield(ls, -2, "length");

    lua_pushcfunction(ls, luaw_vecrotate);
    lua_setfield(ls, -2, "rotate");

    lua_newtable(ls);

    lua_pushcfunction(ls, luaw_vecadd);
    lua_setfield(ls, -2, "__add");

    lua_pushcfunction(ls, luaw_vecsub);
    lua_setfield(ls, -2, "__sub");

    lua_pushcfunction(ls, luaw_vecmul);
    lua_setfield(ls, -2, "__mul");

    lua_pushcfunction(ls, luaw_vecdiv);
    lua_setfield(ls, -2, "__div");

    lua_setmetatable(ls, -2);
}


static int luaw_veclength(lua_State *ls)
{
    lua_pushnumber(ls, lua_tovector(ls, 1).length());
    return 1;
}


static int luaw_vecrotate(lua_State *ls)
{
    mat4 rot = mat4::identity().rotate(lua_tonumber(ls, 3), lua_tovector(ls, 2));
    lua_pushvector(ls, vec3(rot * vec4::direction(lua_tovector(ls, 1))));
    return 1;
}


static int luaw_vecadd(lua_State *ls)
{
    lua_pushvector(ls, lua_tovector(ls, 1) + lua_tovector(ls, 2));
    return 1;
}


static int luaw_vecsub(lua_State *ls)
{
    lua_pushvector(ls, lua_tovector(ls, 1) - lua_tovector(ls, 2));
    return 1;
}


static int luaw_vecmul(lua_State *ls)
{
    if (lua_isnumber(ls, 1)) {
        lua_pushvector(ls, static_cast<float>(lua_tonumber(ls, 1)) * lua_tovector(ls, 2));
    } else {
        lua_pushvector(ls, static_cast<float>(lua_tonumber(ls, 2)) * lua_tovector(ls, 1));
    }
    return 1;
}


static int luaw_vecdiv(lua_State *ls)
{
    lua_pushvector(ls, lua_tovector(ls, 1) / lua_tonumber(ls, 2));
    return 1;
}


static int luaw_crossp(lua_State *ls)
{
    lua_pushvector(ls, lua_tovector(ls, 1).cross(lua_tovector(ls, 2)));
    return 1;
}


static int luaw_dotp(lua_State *ls)
{
    lua_pushnumber(ls, lua_tovector(ls, 1).dot(lua_tovector(ls, 2)));
    return 1;
}


void FlightControlSoftware::execute(ShipState &ship, const Input &input, float interval)
{
    lua_getglobal(ls(), "flight_control");
    if (lua_isnil(ls(), -1)) {
        throw std::runtime_error(enm() + ": flight_control undefined");
    }


    lua_newtable(ls());

    lua_pushvector(ls(), ship.local_acceleration);
    lua_setfield(ls(), -2, "acceleration");

    lua_pushvector(ls(), ship.local_rotational_velocity);
    lua_setfield(ls(), -2, "rotational_velocity");

    lua_pushnumber(ls(), ship.total_mass);
    lua_setfield(ls(), -2, "total_mass");

    lua_newtable(ls());

    for (size_t i = 0; i < ship.ship->thrusters.size(); i++) {
        const Thruster &thr = ship.ship->thrusters[i];

        lua_pushinteger(ls(), i);
        lua_newtable(ls());

        lua_pushvector(ls(), thr.force);
        lua_setfield(ls(), -2, "force");

        lua_pushvector(ls(), thr.relative_position);
        lua_setfield(ls(), -2, "relative_position");

        lua_pushinteger(ls(), thr.type);
        lua_setfield(ls(), -2, "type");

        lua_pushinteger(ls(), thr.general_position);
        lua_setfield(ls(), -2, "general_position");

        lua_pushinteger(ls(), thr.general_direction);
        lua_setfield(ls(), -2, "general_direction");

        lua_settable(ls(), -3);
    }

    lua_setfield(ls(), -2, "thrusters");


    lua_newtable(ls());

    vec3 rotate(input.get_mapping("rotate.+x") - input.get_mapping("rotate.-x"),
                input.get_mapping("rotate.+y") - input.get_mapping("rotate.-y"),
                input.get_mapping("rotate.+z") - input.get_mapping("rotate.-z"));

    lua_pushvector(ls(), rotate);
    lua_setfield(ls(), -2, "rotate");

    vec3 strafe(input.get_mapping("strafe.+x") - input.get_mapping("strafe.-x"),
                input.get_mapping("strafe.+y") - input.get_mapping("strafe.-y"),
                input.get_mapping("strafe.+z") - input.get_mapping("strafe.-z"));

    lua_pushvector(ls(), strafe);
    lua_setfield(ls(), -2, "strafe");

    lua_pushnumber(ls(), input.get_mapping("+main_engine") -
                         input.get_mapping("-main_engine"));
    lua_setfield(ls(), -2, "main_engine");

    lua_pushboolean(ls(), input.get_mapping("kill_rotation") >= .5f);
    lua_setfield(ls(), -2, "kill_rotation");


    lua_pushnumber(ls(), interval);


    lua_call(ls(), 3, 1);


    if (lua_isnil(ls(), -1)) {
        lua_pop(ls(), 1);
        return;
    }

    for (size_t i = 0; i < ship.thruster_states.size(); i++) {
        lua_pushinteger(ls(), i);
        lua_gettable(ls(), -2);

        if (lua_isnumber(ls(), -1)) {
            ship.thruster_states[i] += lua_tonumber(ls(), -1);
        } else if (!lua_isnil(ls(), -1)) {
            throw std::runtime_error(enm() + ": Bad thruster state returned");
        }

        lua_pop(ls(), 1);
    }

    lua_pop(ls(), 1);
}


int ScenarioScript::luaw_enable_player_physics(lua_State *ls)
{
    enable_player_physics(lua_toboolean(ls, 1));
    return 0;
}


int ScenarioScript::luaw_fix_player_to_ground(lua_State *ls)
{
    fix_player_to_ground(lua_toboolean(ls, 1));
    return 0;
}


static ShipState *lua_toship(lua_State *ls, int index)
{
    lua_getfield(ls, index, "id");
    ShipState *ship = static_cast<ShipState *>(lua_touserdata(ls, -1));
    lua_pop(ls, 1);

    return ship;
}


int ScenarioScript::luaw_set_ship_position(lua_State *ls)
{
    ScenarioScript *ss =
        static_cast<ScenarioScript *>(lua_touserdata(ls, lua_upvalueindex(1)));
    ShipState *ship = lua_toship(ls, 1);
    float lng = lua_tonumber(ls, 2) - M_PIf / 2.f;
    float lat = lua_tonumber(ls, 3);
    float height = lua_tonumber(ls, 4);

    ship->position = ss->current_world_state->earth_mv
                     * vec4(cosf(lat) * sinf(lng),
                            sinf(lat),
                            cosf(lat) * cosf(lng),
                            1.f);
    ship->position += static_cast<double>(height) * ship->position.normalized();

    return 0;
}


int ScenarioScript::luaw_set_ship_velocity(lua_State *ls)
{
    ShipState *ship = lua_toship(ls, 1);

    ship->velocity = ship->right   * lua_tonumber(ls, 2)
                   + ship->up      * lua_tonumber(ls, 3)
                   + ship->forward * lua_tonumber(ls, 4);

    return 0;
}


int ScenarioScript::luaw_set_ship_bearing(lua_State *ls)
{
    ScenarioScript *ss =
        static_cast<ScenarioScript *>(lua_touserdata(ls, lua_upvalueindex(1)));
    ShipState *ship = lua_toship(ls, 1);
    vec3 tangent = vec3(ss->current_world_state->earth_mv
                        * vec4(0.f, 1.f, 0.f, 0.f))
                   .cross(ship->position).normalized();

    ship->up      = ship->position.normalized();
    ship->forward = vec3(mat4::identity().rotated(lua_tonumber(ls, 2), ship->up)
                         * vec4::direction(tangent));
    ship->right   = ship->forward.cross(ship->up);

    return 0;
}


void ScenarioScript::lua_pushship(ShipState *ship)
{
    lua_newtable(ls());

    lua_pushlightuserdata(ls(), ship);
    lua_setfield(ls(), -2, "id");

    lua_pushlightuserdata(ls(), this);
    lua_pushcclosure(ls(), ScenarioScript::luaw_set_ship_position, 1);
    lua_setfield(ls(), -2, "set_position");

    lua_pushlightuserdata(ls(), this);
    lua_pushcclosure(ls(), ScenarioScript::luaw_set_ship_velocity, 1);
    lua_setfield(ls(), -2, "set_velocity");

    lua_pushlightuserdata(ls(), this);
    lua_pushcclosure(ls(), ScenarioScript::luaw_set_ship_bearing, 1);
    lua_setfield(ls(), -2, "set_bearing");
}


int ScenarioScript::luaw_player_ship(lua_State *ls)
{
    ScenarioScript *ss =
        static_cast<ScenarioScript *>(lua_touserdata(ls, lua_upvalueindex(1)));
    int psi = ss->current_world_state->player_ship;

    ss->lua_pushship(&ss->current_world_state->ships[psi]);

    return 1;
}


int ScenarioScript::luaw_spawn_ship(lua_State *ls)
{
    ScenarioScript *ss =
        static_cast<ScenarioScript *>(lua_touserdata(ls, lua_upvalueindex(1)));
    const char *ship_name = lua_tolstring(ls, 1, nullptr);
    const Ship *ship_type = ship_types[ship_name];

    if (!ship_type) {
        throw std::runtime_error("Unknown ship type “" + std::string(ship_name)
                                 + "”");
    }

    ShipState &ns = ss->current_world_state->spawn_ship(ship_type);
    ss->lua_pushship(&ns);

    return 1;
}


void ScenarioScript::initialize(WorldState &state)
{
    lua_pushcfunction(ls(), ScenarioScript::luaw_enable_player_physics);
    lua_setglobal(ls(), "enable_player_physics");

    lua_pushcfunction(ls(), ScenarioScript::luaw_fix_player_to_ground);
    lua_setglobal(ls(), "fix_player_to_ground");

    lua_pushlightuserdata(ls(), this);
    lua_pushcclosure(ls(), ScenarioScript::luaw_player_ship, 1);
    lua_setglobal(ls(), "player_ship");

    lua_pushlightuserdata(ls(), this);
    lua_pushcclosure(ls(), ScenarioScript::luaw_spawn_ship, 1);
    lua_setglobal(ls(), "spawn_ship");

    current_world_state = &state;

    lua_getglobal(ls(), "initialize");
    lua_call(ls(), 0, 0);

    current_world_state = nullptr;
}


void ScenarioScript::execute(WorldState &out_state, const WorldState &in_state, const Input &input)
{
    (void)input;

    ShipState &ops = out_state.ships[out_state.player_ship];
    const ShipState &ips = in_state.ships[in_state.player_ship];

    lua_getglobal(ls(), "step");

    lua_pushnumber(ls(), out_state.interval);

    lua_newtable(ls());

    struct cvecval { const char *name; const vec3 &vec; };
    for (const auto &vec: (cvecval[]){ { "velocity", ips.velocity },
                                       { "up", ips.up }, { "forward", ips.forward }, { "right", ips.right } })
    {
        lua_pushvector(ls(), vec.vec);
        lua_setfield(ls(), -2, vec.name);
    }

    current_world_state = &out_state;
    lua_call(ls(), 2, 1);
    current_world_state = nullptr;

    if (lua_isnil(ls(), -1)) {
        lua_pop(ls(), 1);
        return;
    }

    struct vecval { const char *name; vec3 &vec; };
    for (const auto &vec: (vecval[]){ { "velocity", ops.velocity },
                                      { "up", ops.up }, { "forward", ops.forward }, { "right", ops.right } })
    {
        lua_getfield(ls(), -1, vec.name);
        vec.vec = lua_tovector(ls(), -1);
        lua_pop(ls(), 1);
    }

    if (!ops.up.length()) {
        ops.up = ops.right.cross(ops.forward);
    } else if (!ops.forward.length()) {
        ops.forward = ops.up.cross(ops.right);
    } else if (!ops.right.length()) {
        ops.right = ops.forward.cross(ops.up);
    }

    lua_pop(ls(), 1);
}


template<> FlightControlSoftware &Software::sub<FlightControlSoftware>(void)
{
    if (t != FLIGHT_CONTROL) {
        throw std::runtime_error("Attempted to cast non-flight-control software to FlightControlSoftware");
    }
    return *reinterpret_cast<FlightControlSoftware *>(sw);
}

template<> ScenarioScript &Software::sub<ScenarioScript>(void)
{
    if (t != SCENARIO) {
        throw std::runtime_error("Attempted to cast non-scenario-script to ScenarioScript");
    }
    return *reinterpret_cast<ScenarioScript *>(sw);
}


void load_software(void)
{
    std::string base = gl::find_resource_filename("software/");

    DIR *d = opendir(base.c_str());
    if (!d) {
        throw std::runtime_error("Could not open software directory");
    }

    for (dirent *de = readdir(d); de; de = readdir(d)) {
        if (de->d_name[0] == '.') {
            continue;
        }

        std::string name = base + de->d_name;

        Software *s = new Software(de->d_name, name);
        software[s->type()].push_back(s);

        printf("%s: %s software %s\n", de->d_name, software_type_names[s->type()], s->name().c_str());
    }
}


void execute_flight_control_software(ShipState &ship, const Input &input, float interval)
{
    memset(ship.thruster_states.data(), 0,
           sizeof(ship.thruster_states[0]) * ship.thruster_states.size());

    for (Software *s: software[Software::FLIGHT_CONTROL]) {
        s->sub<FlightControlSoftware>().execute(ship, input, interval);
    }
}


Software *get_scenario(const std::string &name)
{
    for (Software *s: software[Software::SCENARIO]) {
        if (s->name() == name) {
            return s;
        }
    }

    return nullptr;
}
