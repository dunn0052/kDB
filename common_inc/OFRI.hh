#ifndef OFRI__HH
#define OFRI__HH

constexpr unsigned int OBJECT_NAME_LEN = 20;

typedef char OBJECT[OBJECT_NAME_LEN];
typedef unsigned int FIELD;
typedef unsigned int RECORD;
typedef unsigned int INDEX;

struct OFRI
{
    OBJECT o;
    FIELD f;
    RECORD r;
    INDEX i;
};

#endif