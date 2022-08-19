#include <Database.hh>
#include <iostream>
#include <sys/mman.h>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>

#include <allDBs.hh>

Database::Database()
{

}

Database::~Database()
{

}

inline int OpenDatabase(const OBJECT& objectName)
{
   std::stringstream filepath;
    // Get relative path
    filepath << "./db/db/" << objectName << ".db";
    const std::string path = filepath.str();
    int fd = open(path.c_str(), O_RDWR);
    return fd;
}

inline char* MapObject(int fd, off_t size)
{
    char *p_return = static_cast<char*>( mmap(nullptr, size,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, 0) );

    if( p_return == MAP_FAILED )
    {
        return nullptr;
    }

    return p_return;
}

RETCODE Database::Open(const OBJECT& objectName)
{
    int fd = OpenDatabase(objectName);

    RETCODE retcode = RTN_OK;
    if( 0 > fd )
    {
        std::cout << "Failed to open: " << objectName << "\n";
        return RTN_NOT_FOUND;
    }

    struct stat statbuf;
    int error = fstat(fd, &statbuf);
    if( 0 > error )
    {
        return RTN_NOT_FOUND;
    }

    char* p_object = MapObject(fd, statbuf.st_size);
    if( nullptr == p_object )
    {
        std::cout << "Failed to map: " <<  objectName << "\n";
        retcode |= RTN_FAIL;
    }

    error = close(fd);
    if( 0 > error )
    {
        std::cout << "Could not close the database file for: " << objectName << "\n";
        retcode |= RTN_FAIL;
    }

    if(RTN_OK == retcode )
    {
        MappedMemory mem = {.p_mapped = p_object, .size = statbuf.st_size};
        m_ObjectMemMap[objectName] = mem;
    }

    return retcode;
}

RETCODE Database::Close(const OBJECT& objectName)
{
    int error = 0;

    auto mapIterator = m_ObjectMemMap.find(objectName);

    if ( mapIterator != m_ObjectMemMap.end() )
    {
        error = munmap(mapIterator->second.p_mapped, mapIterator->second.size);
        m_ObjectMemMap.erase(mapIterator);

        if( 0 != error )
        {
            return RTN_FAIL;
        }

        return RTN_OK;
    }

    return RTN_FAIL;
}

char* Database::GetObjectMem(const OBJECT& objectName)
{
    auto mapIterator = m_ObjectMemMap.find(objectName);

    if( mapIterator != m_ObjectMemMap.end() )
    {
        return mapIterator->second.p_mapped;
    }

    return nullptr;
}
