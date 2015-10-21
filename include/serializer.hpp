#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <memory>
#include <string>

#include "generic-data.hpp"
#include "json.hpp"
#include "json-structs.hpp"


template<typename T> T *parse(T *destination, const std::string &json)
{
    bool allocate = !destination;

    std::unique_ptr<GDData> g(json_parse(json.c_str()));

    if (allocate) {
        destination = new T;
    }

    try {
        parse(destination, g.get());
    } catch (...) {
        if (allocate) {
            delete destination;
        }
        throw;
    }

    return destination;
}

template<typename T> T *parse(const std::string &json)
{
    return parse<T>(nullptr, json);
}

template<typename T> T *parse_file(T *destination, const std::string &name)
{
    bool allocate = !destination;

    std::unique_ptr<GDData> g(json_parse_file(name));

    if (allocate) {
        destination = new T;
    }

    try {
        parse(destination, g.get());
    } catch (...) {
        if (allocate) {
            delete destination;
        }
        throw;
    }

    return destination;
}

template<typename T> T *parse_file(const std::string &name)
{
    return parse_file<T>(nullptr, name);
}

#endif
