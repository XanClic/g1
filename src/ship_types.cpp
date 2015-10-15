#include <stdexcept>
#include <string>
#include <unordered_map>

#include "generic-data.hpp"
#include "json.hpp"
#include "json-structs.hpp"
#include "ship_types.hpp"


std::unordered_map<std::string, const Ship *> ship_types;


void load_ship_types(void)
{
    GDData *ship_index = json_parse_file("config/ships/index.json");
    try {
        if (ship_index->type != GDData::ARRAY) {
            throw std::runtime_error("config/ships/index.json must contain an "
                                     "array of strings");
        }

        const GDArray &ship_array = *ship_index;
        for (const GDData &ship: ship_array) {
            if (ship.type != GDData::STRING) {
                throw std::runtime_error("config/ships/index.json must contain "
                                         "an array of strings");
            }

            const std::string &name = ship;
            GDData *ship_spec = json_parse_file("config/ships/" + name
                                                + ".json");
            Ship *s = new Ship;
            try {
                Ship_parse(s, ship_spec);
            } catch (...) {
                delete s;
                delete ship_spec;
                throw;
            }

            delete ship_spec;

            ship_types[name] = s;
        }
    } catch (...) {
        for (auto &p: ship_types) {
            delete p.second;
        }
        ship_types.clear();
        delete ship_index;
        throw;
    }

    delete ship_index;
}
