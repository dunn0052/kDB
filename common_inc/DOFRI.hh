#ifndef DOFRI__HH
#define DOFRI__HH

#include <string>

constexpr size_t DATABASE_NAME_LEN = 20;
constexpr size_t OBJECT_NAME_LEN = 20;
constexpr size_t FIELD_NAME_LEN = 20;

typedef char DATABASE[DATABASE_NAME_LEN];
typedef char OBJECT[OBJECT_NAME_LEN];
typedef char FIELD[FIELD_NAME_LEN];
typedef unsigned int RECORD;
typedef unsigned int INDEX;

struct DOFRI
{
    DATABASE d;
    OBJECT o;
    FIELD f;
    RECORD r;
    INDEX i;
};

#endif