#include "DOFRI.hh"
#include "retcode.hh"
#include "demangler.hh"

#include <cstring>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>

template <class ObjectType>
class DatabaseObj
{
    char* m_DBAddress;
    size_t m_size;
    std::string m_ObjectName;
    bool m_IsOpen;

    public:
        DatabaseObj()
            : m_DBAddress(nullptr), m_Size(0), m_ObjectName(type_name<ObjectType>()),
              m_ObjectName(), m_IsOpen(false)
        {
            // Intentionally blank
        }
        ~DatabaseObj() { }

        ObjectType* Get(const RECORD record)
        {
            if(nullptr != m_DBAddress)
            {
                size_t byte_index = sizeof(ObjectType) * record;
                if( m_size > byte_index )
                {
                    return reinterpret_cast<ObjectType*>(m_DBAddress + sizeof(ObjectType) * record);
                }
            }

            return nullptr;
        }


        RETCODE Open()
        {
            int fd = OpenDatabase();
            RETCODE retcode = RTN_OK;
            if( 0 > fd )
            {
                std::cout << "Failed to open: " << m_ObjectName << "\n";
                return RTN_NOT_FOUND;
            }

            struct stat statbuf;
            int error = fstat(fd, &statbuf);
            if( 0 > error )
            {
                return RTN_NOT_FOUND;
            }

            error = close(fd);
            if( 0 > error )
            {
                std::cout << "Could not close the database file for: " << m_ObjectName << "\n";
                retcode |= RTN_FAIL;
            }

            m_size = statbuf.st_size;
            m_DBAddress = MapObject(fd, m_size);
            if( nullptr == m_DBAddress )
            {
                std::cout << "Failed to map: " <<  m_ObjectName << "\n";
                retcode |= RTN_FAIL;
            }

            return retcode;
        }

        RETCODE Close()
        {
            int error = 0;

            error = munmap(m_DBAddress, m_size);
            if( 0 != error )
            {
                return RTN_FAIL;
            }

            m_IsOpen = false;
            return RTN_OK;
        }

        RETCODE UpdateRecord(const ObjectType& update, const size_t&& record)
        {
            ObjectType* object = Get(record);
            if(NULL != object)
            {
                memcpy(object, &update, sizeof(ObjectType));
                notify(record);
                return RTN_OK;
            }

            return RTN_NOT_FOUND;
        }

        RETCODE Subscribe(const size_t record)
        {

        }

    private:



        char* GetObjectMem()
        {
            auto mapIterator = m_ObjectMemMap.find(objectName);

            if( mapIterator != m_ObjectMemMap.end() )
            {
                return mapIterator->second.p_mapped;
            }
            else
            {
                // Attempt to open object
                RETCODE retcode = Open(objectName);
                if( RTN_OK == retcode )
                {
                    // Hopefully we don't get some weird recursive bs
                    // But after opening the object should be mapped
                    return GetObjectMem(objectName);
                }
            }

            return nullptr;
        }

    int OpenDatabase(const OBJECT& objectName)
    {
        std::stringstream filepath;
        // @TODO Get relative path
        filepath << "./db/db/" << objectName << ".db";
        const std::string path = filepath.str();
        int fd = open(path.c_str(), O_RDWR);
        return fd;
    }

};