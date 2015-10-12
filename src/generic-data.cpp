#include <stdexcept>

#include "generic-data.hpp"


GDData::GDData(const GDData &d)
{
    type = d.type;
    switch (type) {
        case NIL:
            break;

        case BOOLEAN:
            b = d.b;
            break;

        case INTEGER:
            i = d.i;
            break;

        case FLOAT:
            f = d.f;
            break;

        case STRING:
            new (&s) GDString(d.s);
            break;

        case ARRAY:
            new (&a) GDArray(d.a);
            break;

        case OBJECT:
            new (&o) GDObject;
            for (const auto &pair: d.o) {
                o[pair.first] = new GDData(*pair.second);
            }
            break;

        default:
            type = NIL;
            throw std::runtime_error("Invalid GDData type");
    }
}


GDData::~GDData(void)
{
    Type old_type = type;
    type = NIL;

    switch (old_type) {
        case NIL:
        case BOOLEAN:
        case INTEGER:
        case FLOAT:
            break;

        case STRING:
            s.~GDString();
            break;

        case ARRAY:
            a.~GDArray();
            break;

        case OBJECT:
            for (auto &pair: o) {
                delete pair.second;
                pair.second = nullptr;
            }
            o.~GDObject();
            break;
    }
}


GDData &GDData::operator=(const GDData &d)
{
    this->~GDData();

    new (this) GDData(d);

    return *this;
}


GDData::operator GDInteger(void) const
{
    if (type != INTEGER) {
        throw std::runtime_error("GDData object is not an integer");
    }
    return i;
}

GDData::operator GDFloat(void) const
{
    if (type != FLOAT) {
        throw std::runtime_error("GDData object is not a float");
    }
    return f;
}

GDData::operator const GDString &(void) const
{
    if (type != STRING) {
        throw std::runtime_error("GDData object is not a string");
    }
    return s;
}

GDData::operator GDBoolean(void) const
{
    if (type != BOOLEAN) {
        throw std::runtime_error("GDData object is not a boolean");
    }
    return b;
}

GDData::operator GDNil(void) const
{
    if (type != NIL) {
        throw std::runtime_error("GDData object is not null");
    }
    return nullptr;
}

GDData::operator const GDArray &(void) const
{
    if (type != ARRAY) {
        throw std::runtime_error("GDData object is not an array");
    }
    return a;
}

GDData::operator const GDObject &(void) const
{
    if (type != OBJECT) {
        throw std::runtime_error("GDData object is not an object");
    }
    return o;
}
