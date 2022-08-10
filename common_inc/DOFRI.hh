#ifndef DOFRI__HH
#define DOFRI__HH

#include <string>

typedef std::string DATABASE;
typedef std::string OBJECT;
typedef std::string FIELD;
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