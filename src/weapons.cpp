#include <vector>

#include "generic-data.hpp"
#include "json.hpp"
#include "json-structs.hpp"
#include "weapons.hpp"


std::vector<const WeaponClass *> weapon_classes;


void load_weapons(void)
{
    GDData *weapon_type_index = json_parse_file("assets/weapons/index.json");
    try {
        if (weapon_type_index->type != GDData::ARRAY) {
            throw std::runtime_error("assets/weapons/index.json must contain "
                                     "an array of strings");
        }

        const GDArray &weapon_type_array = *weapon_type_index;
        for (const GDData &weapon_type: weapon_type_array) {
            if (weapon_type.type != GDData::STRING) {
                throw std::runtime_error("assets/weapons/index.json must "
                                         "contain an array of strings");
            }

            const std::string &type = weapon_type;
            GDData *weapon_class_spec = json_parse_file("assets/weapons/" + type
                                                        + ".json");
            WeaponClass *wc = new WeaponClass;
            try {
                WeaponClass_parse(wc, weapon_class_spec);
            } catch (...) {
                delete wc;
                delete weapon_class_spec;
                throw;
            }

            delete weapon_class_spec;

            if (weapon_classes.size() <= wc->type) {
                weapon_classes.resize(wc->type + 1, nullptr);
            }
            weapon_classes[wc->type] = wc;
        }
    } catch (...) {
        for (const WeaponClass *wc: weapon_classes) {
            delete const_cast<WeaponClass *>(wc);
        }
        weapon_classes.clear();
        delete weapon_type_index;
        throw;
    }

    delete weapon_type_index;
}
