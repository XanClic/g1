#ifndef WEAPONS_HPP
#define WEAPONS_HPP

#include <vector>

#include "json-structs.hpp"


extern std::vector<const WeaponClass *> weapon_classes;


void load_weapons(void);

#endif
