#ifndef JSON_HPP
#define JSON_HPP

#include <string>

#include "generic-data.hpp"


GDData *json_parse(const char *json);
GDData *json_parse_file(const std::string &filename);

#endif
