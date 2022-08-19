#ifndef test__HH
#define test__HH

#include "DOFRI.hh"

#define D_TEST_DB_NUM 1
#define D_TEST_NAME "TEST"


#define O_TEST_1_NUM_RECORDS 100

struct O_TEST_1
{
    RECORD record;
    unsigned int TEST_1;
    unsigned int TEST_2;
    char TEST_3[20];
};

#define O_TEST_2_NUM_RECORDS 98

struct O_TEST_2
{
    RECORD record;
    DATABASE TEST_2;
    unsigned char TEST_3[20];
    char TEST_4;
};

#define O_CHARACTER_NUM_RECORDS 100

struct O_CHARACTER
{
    RECORD record;
    unsigned int STR;
    unsigned int WIS;
    unsigned int INT;
    unsigned int DEX;
};

#define O_WEAPON_NUM_RECORDS 30

struct O_WEAPON
{
    RECORD record;
    unsigned int DMG[2];
    unsigned int WEIGHT;
    char NAME[20];
};

#endif