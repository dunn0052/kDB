#ifndef __DB_SIZER_HH
#define __DB_SIZER_HH

#include <retcode.hh>
#include <DOFRI.hh>
#include <allDBs.hh>
#include <map>

enum OBJECT_ENUM
{
    e_BASS = 1
};

static std::map<std::string, OBJECT_ENUM>  objects
{
    {"BASS", e_BASS}
};

static RETCODE ObjectSize(const OBJECT& object_name, size_t& out_size)
{
    out_size = 0;

    switch(objects[std::string(object_name)])
    {
        case e_BASS:
            return sizeof(BASS);
        default:
            break;
    }
    return RTN_NOT_FOUND;
}


#endif