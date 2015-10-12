#ifndef GENERIC_DATA_HPP
#define GENERIC_DATA_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


struct GDData;

typedef int64_t GDInteger;
typedef double GDFloat;
typedef std::string GDString;
typedef bool GDBoolean;
typedef void *GDNil;
typedef std::vector<GDData> GDArray;
typedef std::unordered_map<GDString, GDData *> GDObject;

struct GDData {
    enum Type {
        NIL,
        BOOLEAN,
        INTEGER,
        FLOAT,
        STRING,
        ARRAY,
        OBJECT
    } type;

    union {
        GDInteger i;
        GDFloat f;
        GDString s;
        GDBoolean b;
        GDArray a;
        GDObject o;
    };

    GDData(void): type(NIL) {}
    GDData(Type t): type(t) {}
    GDData(const GDData &d);
    ~GDData(void);

    GDData &operator=(const GDData &d);

    operator GDInteger(void) const;
    operator GDFloat(void) const;
    operator const GDString &(void) const;
    operator GDBoolean(void) const;
    operator GDNil(void) const;
    operator const GDArray &(void) const;
    operator const GDObject &(void) const;
};

#endif
