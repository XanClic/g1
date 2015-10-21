#include <vector>

#include "serializer.hpp"
#include "weapons.hpp"


std::vector<const WeaponClass *> weapon_classes;


static void do_load_weapons(void)
{
    WeaponClassIndex wci;
    parse_file(&wci, "config/weapons/index.json");

    for (const std::string &type: wci.classes) {
        WeaponClass *wc = parse_file<WeaponClass>("config/weapons/" + type +
                                                  ".json");

        if (weapon_classes.size() <= wc->type) {
            weapon_classes.resize(wc->type + 1, nullptr);
        }
        weapon_classes[wc->type] = wc;
    }
}


void load_weapons(void)
{
    try {
        do_load_weapons();
    } catch (...) {
        for (const WeaponClass *wc: weapon_classes) {
            delete const_cast<WeaponClass *>(wc);
        }
        weapon_classes.clear();
        throw;
    }
}
