#ifndef OFRI__HH
#define OFRI__HH

#include <string>
#include <cstring>

constexpr unsigned int OBJECT_NAME_LEN = 20;

typedef char OBJECT[OBJECT_NAME_LEN];
typedef unsigned int FIELD;
typedef unsigned int RECORD;
typedef unsigned int INDEX;

struct OFRI // Specific quanta of data
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