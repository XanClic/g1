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

#include "physics.hpp"
#include "ship.hpp"
#include "software.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


static const char *const software_type_names[] = {
    "flight control"
};

static std::vector<Software *> software[Software::TYPE_MAX];


Software::Software(const std::string &n, const std::string &filename):
    enm(n)
{
    ls = luaL_newstate();

    luaopen_base(ls);
    luaopen_table(ls);
    luaopen_string(ls);
    luaopen_math(ls);

    if (luaL_loadfile(ls, filename.c_str())) {
        throw std::runtime_error("Could not load lua script " + filename + ": " + std::string(lua_tostring(ls, -1)));
    }


    sg("FLIGHT_CONTROL", FLIGHT_CONTROL);

    sg("RCS",  Thruster::RCS);
    sg("MAIN", Thruster::MAIN);

    sg("RIGHT",    Thruster::RIGHT);
    sg("LEFT",     Thruster::LEFT);
    sg("UP",       Thruster::UP);
    sg("DOWN",     Thruster::DOWN);
    sg("FORWARD",  Thruster::FORWARD);
    sg("BACKWARD", Thruster::BACKWARD);
    sg("TOP",      Thruster::TOP);
    sg("BOTTOM",   Thruster::BOTTOM);
    sg("FRONT",    Thruster::FRONT);
    sg("BACK",     Thruster::BACK);


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
}


void Software::sg(const char *n, int v)
{
    lua_pushinteger(ls, v);
    lua_setglobal(ls, n);
}


static void lua_pushvector(lua_State *ls, const vec3 &vec)
{
    lua_newtable(ls);

    lua_pushstring(ls, "x");
    lua_pushnumber(ls, vec.x());
    lua_settable(ls, -3);

    lua_pushstring(ls, "y");
    lua_pushnumber(ls, vec.y());
    lua_settable(ls, -3);

    lua_pushstring(ls, "z");
    lua_pushnumber(ls, vec.z());
    lua_settable(ls, -3);
}


void Software::execute(ShipState &ship, const Input &input)
{
    lua_getglobal(ls, "flight_control");
    if (lua_isnil(ls, -1)) {
        throw std::runtime_error(enm + ": flight_control undefined");
    }


    lua_newtable(ls);

    lua_pushstring(ls, "acceleration");
    lua_pushvector(ls, ship.acceleration);
    lua_settable(ls, -3);

    lua_pushstring(ls, "rotational_velocity");
    lua_pushvector(ls, ship.rotational_velocity);
    lua_settable(ls, -3);

    lua_pushstring(ls, "total_mass");
    lua_pushnumber(ls, ship.total_mass);
    lua_settable(ls, -3);

    lua_pushstring(ls, "thrusters");
    lua_newtable(ls);

    for (size_t i = 0; i < ship.ship->thrusters.size(); i++) {
        const Thruster &thr = ship.ship->thrusters[i];

        lua_pushunsigned(ls, i);
        lua_newtable(ls);

        lua_pushstring(ls, "force");
        lua_pushvector(ls, thr.force);
        lua_settable(ls, -3);

        lua_pushstring(ls, "relative_position");
        lua_pushvector(ls, thr.relative_position);
        lua_settable(ls, -3);

        lua_pushstring(ls, "type");
        lua_pushinteger(ls, thr.type);
        lua_settable(ls, -3);

        lua_pushstring(ls, "general_position");
        lua_pushinteger(ls, thr.general_position);
        lua_settable(ls, -3);

        lua_pushstring(ls, "general_direction");
        lua_pushinteger(ls, thr.general_direction);
        lua_settable(ls, -3);

        lua_settable(ls, -3);
    }

    lua_settable(ls, -3);


    lua_newtable(ls);

    vec3 pyr(input.pitch, input.yaw, input.roll);
    lua_pushstring(ls, "rotate");
    lua_pushvector(ls, pyr);
    lua_settable(ls, -3);

    vec3 strafe(input.strafe_x, input.strafe_y, input.strafe_z);
    lua_pushstring(ls, "strafe");
    lua_pushvector(ls, strafe);
    lua_settable(ls, -3);

    lua_pushstring(ls, "main_engine");
    lua_pushnumber(ls, input.main_engine);
    lua_settable(ls, -3);


    lua_call(ls, 2, 1);


    for (size_t i = 0; i < ship.thruster_states.size(); i++) {
        lua_pushunsigned(ls, i);
        lua_gettable(ls, -2);

        if (lua_isnil(ls, -1)) {
            ship.thruster_states[i] = 0.f;
        } else if (lua_isnumber(ls, -1)) {
            ship.thruster_states[i] = lua_tonumber(ls, -1);
        } else {
            throw std::runtime_error(enm + ": Bad thruster state returned");
        }

        lua_pop(ls, 1);
    }

    lua_pop(ls, 1);
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


void execute_software(ShipState &ship, const Input &input)
{
    for (Software *s: software[Software::FLIGHT_CONTROL]) {
        s->execute(ship, input);
    }
}
