#include <CLI.hh>
#include <OFRI.hh>
#include <DBMap.hh>
#include <Constants.hh>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <Logger.hh>
#include <ConfigValues.hh>
#include <DBHeader.hh>

#include <pthread_profiler.hh>

static RETCODE GenerateDatabaseFile(const OBJECT& object_name, const std::string& dbPath)
{
    PROFILE_FUNCTION();
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << dbPath <<  object_name << DB_EXT;
    const std::string& path = filepath.str();
    size_t fileSize = 0;

    std::map<std::string, OBJECT_SCHEMA>::iterator it = dbSizes.find(std::string(object_name));
    if(it != dbSizes.end())
    {
        //element found;
        fileSize = sizeof(DBHeader) + it->second.objectSize * it->second.numberOfRecords;
    }
    else
    {
        LOG_WARN("Could not find ", object_name, " in DBMap.hh! Run Schema tool again");
        return RTN_NOT_FOUND;
    }

    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if( 0 > fd )
    {
        LOG_WARN("Failed to open or create ", path);
        retcode |=  RTN_NOT_FOUND;
    }

    if( ftruncate64(fd, fileSize) )
    {
        LOG_WARN("Failed to truncate ", path, " to size ", fileSize);
        retcode |= RTN_MALLOC_FAIL;
    }

    DBHeader dbHeader = {0};
    dbHeader.m_NumRecords = it->second.numberOfRecords;
    strncpy(dbHeader.m_ObjectName, object_name, sizeof(OBJECT));

    pthread_mutexattr_t dbLockAttributes = {0};
    pthread_mutexattr_init(&dbLockAttributes);
    pthread_mutexattr_setpshared(&dbLockAttributes, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&dbHeader.m_DBLock, &dbLockAttributes);
    pthread_mutexattr_destroy(&dbLockAttributes);

    size_t numbytes = write(fd, static_cast<void*>(&dbHeader), sizeof(DBHeader));

    if(sizeof(DBHeader) != numbytes)
    {
        LOG_ERROR("Failed to create DB header for: ", object_name);
        retcode |= RTN_EOF;
    }

    if( close(fd) )
    {
        LOG_WARN("Failed to close ", path);
        retcode |= RTN_FAIL;
    }

    return retcode;
}


int main(int argc, char* argv[])
{
    PROFILE_FUNCTION();
    CLI::Parser parse("InstantiateDB", "Generate and update db files");
    CLI::CLI_OBJECTArgument objectArg("--object", "Name of db object to generate");
    CLI::CLI_FlagArgument allArg("-a", "Generate all db files");
    parse.AddArg(objectArg).AddArg(allArg);

    parse.ParseCommandLineArguments(argc, argv);

    if(objectArg.IsInUse())
    {
        RETCODE retcode = RTN_OK;
        std::string INSTALL_DIR =
            ConfigValues::Instance().Get(KDB_INSTALL_DIR);
        if("" == INSTALL_DIR)
        {
            return RTN_NOT_FOUND;
        }
        std::string db_path = INSTALL_DIR + DB_DB_DIR;
        retcode = GenerateDatabaseFile(objectArg.GetValue(), db_path);
        if(IS_RETCODE_OK(retcode))
        {
            LOG_INFO("Generated ", db_path, objectArg.GetValue(), ".db");
        }
        else
        {
            LOG_WARN("Failed to generate ", db_path.c_str(), objectArg.GetValue(), ".db");
        }

        return RTN_OK;
    }
    else if(allArg.IsInUse())
    {
        RETCODE retcode = RTN_OK;
        std::string INSTALL_DIR =
            ConfigValues::Instance().Get(KDB_INSTALL_DIR);
        if("" == INSTALL_DIR)
        {
            return RTN_NOT_FOUND;
        }
        std::string db_path = INSTALL_DIR + DB_DB_DIR;
        OBJECT current_object = {0};
        std::map<std::string, OBJECT_SCHEMA>::iterator it;
        for (it = dbSizes.begin(); it != dbSizes.end(); it++)
        {
            strncpy(current_object, it->first.c_str(), sizeof(current_object));
            retcode = GenerateDatabaseFile(current_object, db_path);
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Generated ", db_path, current_object, ".db");
            }
            else
            {
                LOG_WARN("Failed to generate ", db_path.c_str(), current_object, ".db");
            }
        }

        return RTN_OK;
    }
    else
    {
        parse.Usage();
    }

    return RTN_FAIL;
}