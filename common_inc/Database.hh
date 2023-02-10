#include <OFRI.hh>
#include <retcode.hh>
#include <demangler.hh>

#include <cstring>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>

static const std::string DB_EXT = ".db";

struct MappedMemory
{
    char* p_mapped;
    off_t size;
};

class Database
{
    public:
        Database(const std::string& file_path);
        ~Database();

        template <typename OBJ_TYPE>
        OBJ_TYPE* Get(const RECORD record)
        {
            char* p_object_memory = GetObjectMem(type_name<OBJ_TYPE>().c_str());
            if(nullptr != p_object_memory)
            {
                return reinterpret_cast<OBJ_TYPE*>(p_object_memory + sizeof(OBJ_TYPE) * record );
            }

            return nullptr;
        }

        template<typename OBJ_TYPE>
        RETCODE ResizeObject(const RECORD numRecords)
        {
            RETCODE retcode = RTN_OK;
            std::stringstream filepath;
            filepath << "./db/"; /* <<  type_name<OBJ_TYPE>() << ".db";*/
            const std::string path = filepath.str();
            size_t fileSize = sizeof(OBJ_TYPE) * numRecords;

            int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
            if( 0 > fd )
            {
                return  RTN_NOT_FOUND;
            }

            if( ftruncate64(fd, fileSize) )
            {
                retcode |= RTN_MALLOC_FAIL;
            }

            if( close(fd) )
            {
                retcode |= RTN_FAIL;
            }

            return retcode;
        }


        RETCODE Open(const OBJECT& objectName);
        RETCODE Close(const OBJECT& objectName);

        template <typename ObjectType>
        RETCODE UpdateRecord(const ObjectType& update, const size_t&& record)
        {
            ObjectType* object = Get<ObjectType>(record);
            if(NULL != object)
            {
                memcpy(object, &update);
                //notify<ObjectType>(record);
                return RTN_OK;
            }

            return RTN_NOT_FOUND;
        }

        template <typename ObjectType>
        RETCODE Subscribe();

    int OpenDatabase(const OBJECT& objectName);

    private:

        std::map<std::string, MappedMemory> m_ObjectMemMap;
        std::string m_DBFilePath;
        char* GetObjectMem(const OBJECT& databaseName);
};