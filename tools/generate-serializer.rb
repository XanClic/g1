#!/usr/bin/env ruby

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

$builtin = ['single', 'vec3', 'string', 'array']
$builtin_type_map = {
    'single' => 'float',
    'vec3'   => 'dake::math::vec3',
    'string' => 'std::string',
    'array'  => :interpret_array
}


def get_complex_type_dependencies(type)
    if type.kind_of?(String)
        deps = [type]
    elsif type.kind_of?(Hash)
        deps = get_complex_type_dependencies(type['type'])
        if type['type'] == 'array'
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
           (struct_order & depends) == depends
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
#{structs.keys * ', '}
EOF
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


def ctype(type)
    if type.kind_of?(String)
        actual_type = type
    elsif type.kind_of?(Hash)
        if !type['type']
            throw "Complex type #{type.inspect} missing a “type” field"
        end

        actual_type = type['type']
    else
        throw "Invalid type specification #{type.inspect}"
    end

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
    fields.map { |name, type|
        "    #{ctype(type)} #{name};"
    } * "\n"
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

#include <dake/math/matrix.hpp>

#include <string>
#include <vector>


// enums

#{cxxenums}


// structs

#{cxxstructs}

#endif
EOF
header.close


serializer = File.open("#{ARGV[1]}/serializer.cpp", 'w')
serializer.write <<EOF
// Auto-generated by tools/generate-serializer.rb; DO NOT EDIT
EOF
serializer.close
