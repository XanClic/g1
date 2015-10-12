#ifndef SHIP_TYPES_HPP
#define SHIP_TYPES_HPP

#include <string>
#include <unordered_map>

#include "json-structs.hpp"


extern std::unordered_map<std::string, const Ship *> ship_types;


void load_ship_types(void);

#endif
