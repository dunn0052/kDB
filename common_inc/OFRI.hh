#ifndef OFRI__HH
#define OFRI__HH

#include <string>
#include <cstring>
#include <ostream>

constexpr unsigned int OBJECT_NAME_LEN = 20;

typedef char OBJECT[OBJECT_NAME_LEN];
typedef unsigned int FIELD;
typedef unsigned int RECORD;
typedef unsigned int INDEX;

/* Address of data in DB */
struct OFRI
{
    OBJECT o;
    FIELD f;
    RECORD r;
    INDEX i;

    bool operator == (const OFRI& other) const
    {
        return !strncmp(o, other.o, sizeof(OBJECT)) &&
                f == other.f &&
                r == other.r &&
                i == other.i;
    }
};

inline std::ostream& operator << (std::ostream& output_stream,
    const OFRI& ofri)
{
    return output_stream
        << ofri.o
        << "."
        << ofri.f
        << "."
        << ofri.r
        << "."
        << ofri.i;
}

struct OR // Refence specific object
{
    OBJECT o;
    RECORD r;
};

namespace std
{
    template<>
    struct hash<OFRI>
    {
        size_t operator() (const OFRI& key) const
        {
            return std::hash<std::string>()(std::string(key.o)) << 1 ^ key.r << 1 ^ key.f << 1 ^ key.i;
        }
    };
}

#endif