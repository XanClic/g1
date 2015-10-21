#include <stdexcept>
#include <string>
#include <unordered_map>

#include "serializer.hpp"
#include "ship_types.hpp"


std::unordered_map<std::string, const Ship *> ship_types;


static void do_load_ship_types(void)
{
    ShipIndex si;
    parse_file(&si, "config/ships/index.json");

    for (const std::string &name: si.types) {
        ship_types[name] = parse_file<Ship>("config/ships/" + name + ".json");
    }
}


void load_ship_types(void)
{
    try {
        do_load_ship_types();
    } catch (...) {
        for (auto &p: ship_types) {
            delete p.second;
        }
        ship_types.clear();
        throw;
    }
}
