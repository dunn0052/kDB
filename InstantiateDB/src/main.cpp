#include <CLI.hh>
#include <OFRI.hh>
#include <DBMap.hh>
#include <Constants.hh>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <Logger.hh>
#include <EnvironmentVariables.hh>

static RETCODE GenerateDatabaseFile(const OBJECT& object_name, const std::string& dbPath)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << dbPath <<  object_name << DB_EXT;
    const std::string& path = filepath.str();
    size_t fileSize = 0;

    std::map<std::string, OBJECT_SCHEMA>::iterator it = dbSizes.find(std::string(object_name));
    if(it != dbSizes.end())
    {
        //element found;
        fileSize = it->second.objectSize * it->second.numberOfRecords;
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

    if( close(fd) )
    {
        LOG_WARN("Failed to close ", path);
        retcode |= RTN_FAIL;
    }

    return retcode;
}


int main(int argc, char* argv[])
{
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
    }
    else
    {
        parse.Usage();
    }

    return RTN_FAIL;
}