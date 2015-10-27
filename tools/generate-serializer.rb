#!/usr/bin/env ruby
# coding: utf-8

require 'json'


Dir.chdir(ARGV[0])

structs = {}
Dir.entries('json-structs').each do |e|
    next unless e =~ /\.json$/
    structs[e.sub(/\.json$/, '')] = JSON.parse(IO.read("json-structs/#{e}"))
end

enums = {}
Dir.entries('json-enums').each do |e|
    next unless e =~ /\.json$/
    enums[e.sub(/\.json$/, '')] = JSON.parse(IO.read("json-enums/#{e}"))
end


def interpret_array(type)
    if !type.kind_of?(Hash) || !type['of']
        throw 'The array type needs an enclosed type'
    end

    "std::vector<#{ctype(type['of'])}>"
end

def interpret_map(type)
    if !type.kind_of?(Hash) || !type['of']
        throw 'The map type needs an enclosed type'
    end

    "std::unordered_map<#{ctype('string')}, #{ctype(type['of'])}>"
end

$builtin = ['single', 'vec3', 'string', 'array', 'map', 'u64', 'u32']
$builtin_type_map = {
    'single' => 'float',
    'vec3'   => 'dake::math::fvec3',
    'string' => 'std::string',
    'array'  => :interpret_array,
    'map'    => :interpret_map,
    'u64'    => 'uint64_t',
    'u32'    => 'uint32_t'
}


def get_complex_type_dependencies(type)
    if type.kind_of?(String)
        deps = [type]
    elsif type.kind_of?(Hash)
        deps = get_complex_type_dependencies(type['type'])
        if type['type'] == 'array' || type['type'] == 'map'
            deps += get_complex_type_dependencies(type['of'])
        end
    else
        throw "Invalid type specification #{type.inspect}"
    end

    return deps - $builtin
end

def get_dependencies(members)
    members.map { |name, type|
        get_complex_type_dependencies(type)
    }.flatten
end


struct_dependencies = {}
structs.each_key do |struct|
    struct_dependencies[struct] =
        get_dependencies(structs[struct]) - enums.keys
end


struct_order = []
while struct_order.size < structs.size
    structs_added = 0
    struct_dependencies.each do |struct, depends|
        if !struct_order.include?(struct) &&
           (depends - (struct_order & depends)).empty?
            struct_order << struct
            structs_added += 1
        end
    end

    break if structs_added == 0
end

if struct_order.size < structs.size
    $stderr.puts <<EOF
Unfulfilled dependencies!
Cannot fulfill:
#{(structs.keys - struct_order) * ', '}
Unfulfilled:
#{(structs.keys - struct_order).map { |s| '- ' + (struct_dependencies[s] - (struct_order & struct_dependencies[s])) * ', ' } * "\n"}
EOF
    exit 1
end


def enum_to_cxx(elements)
    elements.map { |e|
        if e.kind_of?(String)
            "    #{e},"
        elsif e.kind_of?(Hash)
            if !e['name'] || !e['alias']
                throw "Name or alias missing in #{e.inspect}"
            end

            "    #{e['name']} = #{e['alias']},"
        else
            throw "Invalid enum element #{e.inspect}"
        end
    } * "\n"
end

def enums_to_cxx(enums)
    (enums.map { |enum, elements|
        <<EOF
enum #{enum} {
#{enum_to_cxx(elements)}
};
EOF
    } * "\n").strip
end


def first_level_type(type)
    if type.kind_of?(String)
        return type
    elsif type.kind_of?(Hash)
        if !type['type']
            throw "Complex type #{type.inspect} missing a “type” field"
        end

        return type['type']
    else
        throw "Invalid type specification #{type.inspect}"
    end
end

def ctype(type)
    actual_type = first_level_type(type)

    if $builtin_type_map[actual_type]
        builtin = $builtin_type_map[actual_type]
        if builtin.kind_of?(String)
            converted = builtin
        elsif builtin.kind_of?(Symbol)
            converted = Kernel.send(builtin, type)
        else
            throw "Invalid builtin type definition #{builtin.inspect}"
        end
    else
        converted = actual_type
    end

    return converted
end

def struct_to_cxx(fields)
    optional_regex = /^\[(.*)\]$/

    fields.map { |name, type|
        ct = ctype(type)
        plain_name = name.sub(optional_regex, '\1')

        ret = ["    #{ct} #{plain_name};"]
        if name =~ optional_regex
            ret << ["    bool has_#{plain_name};"]
        end

        ret
    }.flatten * "\n"
end

def structs_to_cxx(structs, order)
    (order.map { |struct|
        <<EOF
struct #{struct} {
#{struct_to_cxx(structs[struct])}
};
EOF
    } * "\n").strip
end


cxxenums = enums_to_cxx(enums)

cxxstructs = structs_to_cxx(structs, struct_order)


header = File.open("#{ARGV[1]}/include/json-structs.hpp", 'w')
header.write <<EOF
// Auto-generated by tools/generate-serializer.rb; DO NOT EDIT

#ifndef JSON_STRUCTS_HPP
#define JSON_STRUCTS_HPP

#include <dake/math/fmatrix.hpp>

#include <string>
#include <vector>

#include "generic-data.hpp"


// enums

#{cxxenums}


// structs

#{cxxstructs}


// functions

template<typename T> void parse(T *output, const GDData *input);

#endif
EOF
header.close


serializer = File.open("#{ARGV[1]}/serializer.cpp", 'w')
serializer.write <<EOF
// Auto-generated by tools/generate-serializer.rb; DO NOT EDIT

#include <cstddef>
#include <cstring>
#include <tuple>
#include <stdexcept>
#include <string>

#include "generic-data.hpp"
#include "json.hpp"
#include "json-structs.hpp"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
EOF

enums.each do |name, values|
    serializer.write <<EOF


static const std::pair<#{name}, const char *> #{name}_assoc[] = {
EOF
    values.each do |v|
        if v.kind_of?(String)
            serializer.write("    std::pair<#{name}, const char *>(#{v}, \"#{v}\"),\n")
        else
            serializer.write("    std::pair<#{name}, const char *>(#{v['name']}, \"#{v['name']}\"),\n")
        end
    end
    serializer.write("};\n\n")
    serializer.write <<EOF
template<> void parse<#{name}>(#{name} *obj, const GDData *d)
{
    if (d->type != GDData::STRING) {
        throw std::runtime_error("Enum values (#{name}) must be strings");
    }

    const GDString &s = *d;
    for (size_t i = 0; i < ARRAY_SIZE(#{name}_assoc); i++) {
        if (!strcmp(s.c_str(), #{name}_assoc[i].second)) {
            *obj = static_cast<#{name}>(#{name}_assoc[i].first);
            return;
        }
    }

    throw std::runtime_error("Value specified for #{name} is invalid: " + s);
}
EOF
end

serializer.write <<EOF



template<> void parse<float>(float *obj, const GDData *d)
{
    if (d->type != GDData::FLOAT && d->type != GDData::INTEGER) {
        throw std::runtime_error("Value given for a single is not a float or "
                                 "integer");
    }

    if (d->type == GDData::FLOAT) {
        *obj = (GDFloat)*d;
    } else /* GDData::INTEGER */ {
        *obj = (GDInteger)*d;
    }
}


template<> void parse<dake::math::fvec3>(dake::math::fvec3 *obj, const GDData *d)
{
    if (d->type != GDData::ARRAY) {
        throw std::runtime_error("Value given for vec3 is not an array");
    }

    const GDArray &a = *d;

    if (a.size() != 3) {
        throw std::runtime_error("Array given for vec3 does not have exactly "
                                 "three arguments");
    }

    parse(&obj->x(), &a[0]);
    parse(&obj->y(), &a[1]);
    parse(&obj->z(), &a[2]);
}


template<> void parse<std::string>(std::string *obj, const GDData *d)
{
    if (d->type != GDData::STRING) {
        throw std::runtime_error("Value given for string is not a string");
    }

    *obj = (GDString)*d;
}


template<> void parse<uint64_t>(uint64_t *obj, const GDData *d)
{
    if (d->type != GDData::INTEGER) {
        throw std::runtime_error("Value given for u64 is not an integer");
    }

    *obj = (GDInteger)*d;
}


template<> void parse<uint32_t>(uint32_t *obj, const GDData *d)
{
    if (d->type != GDData::INTEGER) {
        throw std::runtime_error("Value given for u32 is not an integer");
    }

    *obj = (GDInteger)*d;
}


template<typename T> void parse(std::vector<T> *obj, const GDData *d)
{
    if (d->type != GDData::ARRAY) {
        throw std::runtime_error("Value given for an array is not an array");
    }

    const GDArray &a = *d;
    size_t element_count = a.size();
    new (obj) std::vector<T>(element_count);

    for (size_t i = 0; i < element_count; i++) {
        parse(&(*obj)[i], &a[i]);
    }
}


template<typename T> void parse(std::unordered_map<std::string, T> *obj,
                                const GDData *d)
{
    if (d->type != GDData::OBJECT) {
        throw std::runtime_error("Value given for a map is not an object");
    }

    const GDObject &o = *d;
    new (obj) std::unordered_map<std::string, T>;

    for (const auto &p: o) {
        parse(&(*obj)[p.first], p.second);
    }
}
EOF


optional_regex = /^\[(.*)\]$/

structs.each do |struct, fields|
    serializer.write <<EOF


template<> void parse<#{struct}>(#{struct} *obj, const GDData *d)
{
    if (d->type != GDData::OBJECT) {
        throw std::runtime_error("Struct values (#{struct}) must be objects");
    }

    const GDObject &o = *d;
EOF
    fields.each do |name, type|
        plain_name = name.sub(optional_regex, '\1')

        serializer.write <<EOF

    auto #{plain_name}_di = o.find(\"#{plain_name}\");
    if (#{plain_name}_di == o.end()) {
EOF
        if name =~ optional_regex
            serializer.write <<EOF
        obj->has_#{plain_name} = false;
EOF
        else
            serializer.write <<EOF
        throw std::runtime_error("Missing value for #{struct}.#{name}");
EOF
        end
        serializer.write <<EOF
    } else {
EOF
        if name =~ optional_regex
            serializer.write <<EOF
        obj->has_#{plain_name} = true;

EOF
        end
        serializer.write <<EOF
        const GDData *#{plain_name}_d = #{plain_name}_di->second;
        parse(&obj->#{plain_name}, #{plain_name}_d);
    }
EOF
    end
    serializer.write("}\n")
end

serializer.close
