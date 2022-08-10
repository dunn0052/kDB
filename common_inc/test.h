#ifndef TEST__HH
#define TEST__HH

#include <DOFRI.hh>

#define O_TEST_REC_NUM 100

struct O_TEST
{
    RECORD rec;
    unsigned int TEST_1;
    unsigned int TEST_2;
    char TEST_3[20];
};

#endif