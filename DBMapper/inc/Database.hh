#include <DOFRI.hh>
#include <retcode.hh>

#include <cstring>
#include <string>
#include <map>

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

    char* Get(const DOFRI& dofri);
    char* Get(const DATABASE, const OBJECT, const FIELD, const RECORD, const INDEX);

    template <typename OBJ_TYPE>
    OBJ_TYPE* GetObj(const DATABASE d, const RECORD r)
    {
        char* p_database = GetDatabase(d);
        return reinterpret_cast<OBJ_TYPE*>( p_database + sizeof(OBJ_TYPE) * r );
    }

    RETCODE Open(const DATABASE& databaseName);
    RETCODE Close(const DATABASE& databaseName);

    private:

        std::map<DATABASE, MappedMemory> m_DatabaseMaps;
        char* GetDatabase(const DATABASE& databaseName);
};