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

inline int OpenDatabase(const DATABASE& databaseName)
{
   std::stringstream filepath;
    // Get relative path
    filepath << "./db/" << databaseName << ".db";
    const std::string path = filepath.str();
    int fd = open(path.c_str(), O_RDWR);
    return fd;
}

inline char* MapDatabase(int fd, off_t size)
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

RETCODE Database::Open(const DATABASE& databaseName)
{
    int fd = OpenDatabase(databaseName);

    RETCODE retcode = RTN_OK;
    if( 0 > fd )
    {
        std::cout << "Failed to open: " << databaseName << "\n";
        retcode |= RTN_NOT_FOUND;;
    }

    struct stat statbuf;
    int error = fstat(fd, &statbuf);
    if( 0 > error )
    {
        retcode |= RTN_NOT_FOUND;
    }

    char* p_database = MapDatabase(fd, statbuf.st_size);
    if( nullptr == p_database )
    {
        std::cout << "Failed to map: " <<  databaseName << "\n";
        retcode |= RTN_FAIL;
    }

    error = close(fd);
    if( 0 > error )
    {
        std::cout << "Could not close the database file for: " << databaseName << "\n";
        retcode |= RTN_FAIL;
    }

    if(RTN_OK == retcode )
    {
        MappedMemory mem = {.p_mapped = p_database, .size = statbuf.st_size};
        m_DatabaseMaps[databaseName] = mem;
    }


    return retcode;
}

RETCODE Database::Close(const DATABASE& databaseName)
{

    int error = 0;

    auto mapIterator = m_DatabaseMaps.find(databaseName);

    if ( mapIterator != m_DatabaseMaps.end() )
    {
        error = munmap(mapIterator->second.p_mapped, mapIterator->second.size);

        if( 0 != error )
        {
            return RTN_FAIL;
        }

        m_DatabaseMaps.erase(mapIterator);

        return RTN_OK;
    }

    return RTN_FAIL;
}

char* Database::Get(const DOFRI& dofri)
{
    return nullptr;
}

char* Database::Get(const DATABASE d, const OBJECT o, const FIELD f, const RECORD r, const INDEX i)
{
    return nullptr;
}

char* Database::GetDatabase(const DATABASE& databaseName)
{
    auto mapIterator = m_DatabaseMaps.find(databaseName);

    if( mapIterator != m_DatabaseMaps.end() )
    {
        return mapIterator->second.p_mapped;
    }

    return nullptr;
}
