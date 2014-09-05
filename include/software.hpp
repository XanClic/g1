#ifndef SOFTWARE_HPP
#define SOFTWARE_HPP

#include <string>

extern "C" {
#include <lua.h>
}

#include "physics.hpp"
#include "ship.hpp"
#include "ui.hpp"


class Software {
    public:
        enum Type {
            FLIGHT_CONTROL,

            TYPE_MAX
        };

    private:
        lua_State *ls;
        Type t;
        std::string enm, nm;

        void sg(const char *name, int value);

    public:
        Software(const std::string &name, const std::string &filename);

        Type type(void) const { return t; }
        const std::string &name(void) const { return nm; }

        void execute(ShipState &ship, const Input &input);
};


void load_software(void);

void execute_software(ShipState &ship, const Input &input);

#endif
