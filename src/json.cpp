#include <dake/dake.hpp>

#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "generic-data.hpp"


static const char *skip_space(const char *ptr)
{
    while (isspace(*ptr)) {
        ptr++;
    }

    return ptr;
}


static std::runtime_error bad_json(const std::string &reason, const char *data)
{
    char shortened_data[64];

    if (strlen(data) > 63) {
        strncpy(shortened_data, data, 60);
        strcpy(&shortened_data[60], "...");
    } else {
        strcpy(shortened_data, data);
    }

    return std::runtime_error("Bad JSON object: " + reason + ": At: "
                              + shortened_data);
}


static void json_do_parse(const char *json, GDData *d, const char **endp);

static void json_do_parse_object(const char *json, GDData *d, const char **endp)
{
    json = skip_space(json + 1); // '{'

    new (&d->o) GDObject;
    d->type = GDData::OBJECT;

    while (*json && *json != '}') {
        const char *end;
        GDData key, *value;

        json_do_parse(json, &key, &end);
        if (key.type != GDData::STRING) {
            throw bad_json("Keys must be strings", json);
        }

        json = skip_space(end);
        if (*json != ':') {
            throw bad_json("Colon expected", json);
        }
        json = skip_space(json + 1);

        value = new GDData;
        try {
            json_do_parse(json, value, &json);
        } catch (...) {
            delete value;
            throw;
        }

        d->o[key.s] = value;

        json = skip_space(json);
        if (*json != ',' && *json != '}') {
            throw bad_json("Comma or right brace expected", json);
        }
        if (*json == ',') {
            json = skip_space(json + 1);
        }
    }

    if (!*json) {
        throw bad_json("Comma or right brace expected", json);
    }
    *endp = json + 1;
}

static void json_do_parse_array(const char *json, GDData *d, const char **endp)
{
    json = skip_space(json + 1); // '['

    new (&d->a) GDArray;
    d->type = GDData::ARRAY;

    while (*json && *json != ']') {
        d->a.emplace_back();
        json_do_parse(json, &d->a.back(), &json);

        json = skip_space(json);
        if (*json != ',' && *json != ']') {
            throw bad_json("Comma or right bracket expected", json);
        }
        if (*json == ',') {
            json = skip_space(json + 1);
        }
    }

    if (!*json) {
        throw bad_json("Comma or right bracket expected", json);
    }
    *endp = json + 1;
}

static void json_do_parse_number(const char *json, GDData *d, const char **endp)
{
    const char *ptr = json;

    if (*ptr == '-') {
        ptr++;
    }

    while (*ptr && isdigit(*ptr)) {
        ptr++;
    }

    if (*ptr == '.' || *ptr == 'e' || *ptr == 'E') {
        errno = 0;
        double val = strtod(json, const_cast<char **>(endp));
        if (errno) {
            assert(errno == ERANGE);
            throw bad_json("Float out of range", json);
        }

        d->f = val;
        d->type = GDData::FLOAT;
    } else {
        errno = 0;
        long long val = strtoll(json, const_cast<char **>(endp), 10);
        if (errno == ERANGE || val < INT64_MIN || val > INT64_MAX) {
            throw bad_json("Integer out of range", json);
        } else if (errno) {
            throw bad_json("Invalid integer specified", json);
        }

        d->i = val;
        d->type = GDData::INTEGER;
    }
}

static inline int nibble_from_hex(char digit)
{
    return isdigit(digit) ? digit - '0' : tolower(digit) - 'a' + 10;
}

static inline uint16_t unicode_from_hex(const char *ptr)
{
    if (!isxdigit(ptr[0]) || !isxdigit(ptr[1]) ||
        !isxdigit(ptr[2]) || !isxdigit(ptr[3]))
    {
        throw bad_json("Expected 4-hexdigit unicode codepoint", ptr);
    }

    return (nibble_from_hex(ptr[0] << 12))
         | (nibble_from_hex(ptr[1] <<  8))
         | (nibble_from_hex(ptr[2] <<  4))
         |  nibble_from_hex(ptr[3]);
}

static void json_do_parse_string(const char *json, GDData *d, const char **endp)
{
    size_t length = 0;
    const char *ptr = json;

    while (*(++ptr) && *ptr != '"') {
        length++;
        if (*ptr == '\\') {
            ptr++;
            if (!*ptr) {
                throw bad_json("Invalid escape sequence", ptr - 1);
            } else if (*ptr == 'u') {
                uint16_t uni = unicode_from_hex(ptr + 1);
                ptr += 4;

                if (uni >= 0x800) {
                    length += 2;
                } else if (uni >= 0x80) {
                    length += 1;
                }
            }
        }
    }

    if (!*ptr) {
        throw bad_json("String does not terminate", json);
    }


    new (&d->s) GDString(length, 0);
    d->type = GDData::STRING;
    std::string &s = d->s;
    size_t i = 0;

    while (++json < ptr) {
        if (*json != '\\') {
            s[i++] = *json;
        } else {
            switch (*(++json)) {
                case '"':
                case '\\':
                case '/':
                    s[i++] = *json;
                    break;

                case 'b':
                    s[i++] = '\b';
                    break;

                case 'f':
                    s[i++] = '\f';
                    break;

                case 'n':
                    s[i++] = '\n';
                    break;

                case 'r':
                    s[i++] = '\r';
                    break;

                case 't':
                    s[i++] = '\t';
                    break;

                case 'u': {
                    uint16_t uni = unicode_from_hex(ptr + 1);
                    ptr += 4;

                    if (uni < 0x80) {
                        s[i++] = uni;
                    } else if (uni < 0x800) {
                        s[i++] = 0xc0 | (uni >> 6);
                        s[i++] = 0x80 | (uni & 0x3f);
                    } else {
                        s[i++] = 0xe0 | (uni >> 12);
                        s[i++] = 0x80 | ((uni >> 6) & 0x3f);
                        s[i++] = 0x80 | (uni & 0x3f);
                    }

                    break;
                }

                default:
                    throw bad_json("Invalid escape sequence", json - 1);
            }
        }
    }

    assert(i == length);

    *endp = json + 1;
}

static void json_do_parse_boolean(const char *json, GDData *d, const char **endp)
{
    if (!strncmp(json, "false", 5)) {
        *endp = json + 5;
        d->b = false;
        d->type = GDData::BOOLEAN;
    } else if (!strncmp(json, "true", 4)) {
        *endp = json + 4;
        d->b = true;
        d->type = GDData::BOOLEAN;
    } else {
        throw bad_json("Unexpected token", json);
    }
}

static void json_do_parse_null(const char *json, GDData *d, const char **endp)
{
    if (strncmp(json, "null", 4)) {
        throw bad_json("Unexpected token", json);
    }

    *endp = json + 4;
    d->type = GDData::NIL;
}

static void json_do_parse(const char *json, GDData *d, const char **endp)
{
    if (isdigit(*json)) {
        return json_do_parse_number(json, d, endp);
    }

    switch (*json) {
        case '{':
            return json_do_parse_object(json, d, endp);
        case '[':
            return json_do_parse_array(json, d, endp);
        case '"':
            return json_do_parse_string(json, d, endp);
        case '-':
            return json_do_parse_number(json, d, endp);
        case 'n':
            return json_do_parse_null(json, d, endp);
        case 't':
        case 'f':
            return json_do_parse_boolean(json, d, endp);
        default:
            throw bad_json("Unexpected character", json);
    }
}

GDData *json_parse(const char *json)
{
    json = skip_space(json);

    const char *end;
    GDData *d = new GDData;
    try {
        json_do_parse(json, d, &end);
    } catch (...) {
        delete d;
        throw;
    }
    end = skip_space(end);

    if (*end) {
        throw bad_json("Unexpected data after end", end);
    }

    return d;
}

GDData *json_parse_file(const std::string &filename)
{
    std::string full_name = dake::gl::find_resource_filename(filename);
    FILE *fp = fopen(full_name.c_str(), "r");
    if (!fp) {
        throw std::runtime_error("Failed to read " + filename + ": "
                                 + strerror(errno));
    }

    GDData *d;

    // I've always wanted to try this
    try {
        fseek(fp, 0, SEEK_END);
        assert(sizeof(size_t) >= sizeof(long));
        size_t lof = ftell(fp);
        rewind(fp);

        if (lof == SIZE_MAX) {
            throw std::runtime_error("File too big: " + filename);
        }

        char *content = new char[lof + 1];
        try {
            if (fread(content, 1, lof, fp) < lof) {
                throw std::runtime_error("Failed to read data from "
                                         + filename);
            }

            content[lof] = 0;
            d = json_parse(content);
        } catch (...) {
            delete[] content;
            throw;
        }
        delete[] content;
    } catch (...) {
        fclose(fp);
        throw;
    }
    fclose(fp);

    return d;
}
