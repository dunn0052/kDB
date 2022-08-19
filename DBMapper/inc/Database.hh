#include <DOFRI.hh>
#include <retcode.hh>

#include <cstring>
#include <string>
#include <map>
#include <iostream>
#include <demangler.hh>

struct MappedMemory
{
    char* p_mapped;
    off_t size;
};


class Database
{
    public:
        Database();
        ~Database();

    template <typename OBJ_TYPE>
    OBJ_TYPE* Get(const RECORD r)
    {
        std::cout << type_name<OBJ_TYPE>() << "\n";

        char* p_object_memory = GetObjectMem(type_name<OBJ_TYPE>().c_str());
        if(nullptr != p_object_memory)
        {
            return reinterpret_cast<OBJ_TYPE*>(p_object_memory + sizeof(OBJ_TYPE) * r );
        }

        return nullptr;
    }

    RETCODE Open(const OBJECT& objectName);
    RETCODE Close(const OBJECT& objectName);

    private:

        std::map<OBJECT, MappedMemory> m_ObjectMemMap;
        char* GetObjectMem(const OBJECT& databaseName);
};