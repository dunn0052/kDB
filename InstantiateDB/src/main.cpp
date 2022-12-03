#include <CLI.hh>
#include <DOFRI.hh>
#include <DBMap.hh>
#include <Constants.hh>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <Logger.hh>

class CLI_CharArrayTest : public CLI::CLI_Argument<char[20], 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, char (&value)[20])
        {
            memcpy(value, conversion.c_str(), sizeof(value));
            return true;
        }
};

static RETCODE GenerateDatabaseFile(const OBJECT& object_name, const std::string& dbPath)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << dbPath <<  object_name << DB_EXT;
    const std::string& path = filepath.str();
    size_t fileSize = 0;

    std::map<std::string, size_t>::iterator it = dbSizes.find(std::string(object_name));
    if(it != dbSizes.end())
    {
        //element found;
        fileSize = it->second;
    }
    else
    {
        LOG_WARN("Could not find %s in DBMap.hh! Run Schema tool again",
            object_name);
        return RTN_NOT_FOUND;
    }

    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if( 0 > fd )
    {
        LOG_WARN("Failed to open or create %s", path.c_str());
        retcode |=  RTN_NOT_FOUND;
    }

    if( ftruncate64(fd, fileSize) )
    {
        LOG_WARN("Failed to truncate %s to size %u", path.c_str(), fileSize);
        retcode |= RTN_MALLOC_FAIL;
    }

    if( close(fd) )
    {
        LOG_WARN("Failed to close %s", path.c_str());
        retcode |= RTN_FAIL;
    }

    return retcode;
}


int main(int argc, char* argv[])
{
    CLI::Parser parse("InstantiateDB", "Generate and update db files");
    CLI_CharArrayTest objectArg("--object", "Name of db object to generate");
    CLI::CLI_FlagArgument allArg("-a", "Generate all db files");
    parse.AddArg(objectArg).AddArg(allArg);

    parse.ParseCommandLineArguments(argc, argv);

    if(objectArg.IsInUse())
    {
        RETCODE retcode = RTN_OK;
        std::string db_path = INSTALL_DIR + DB_DB_DIR;
        retcode = GenerateDatabaseFile(objectArg.GetValue(), db_path);
        if(IS_RETCODE_OK(retcode))
        {
            LOG_INFO("Generated %s%s.db",
                db_path.c_str(), objectArg.GetValue());
        }
        else
        {
            LOG_WARN("Failed to generate %s%s.db",
                db_path.c_str(), objectArg.GetValue());
        }
    }
    else if(allArg.IsInUse())
    {
        RETCODE retcode = RTN_OK;
        std::string db_path = INSTALL_DIR + DB_DB_DIR;
        OBJECT current_object = {0};
        std::map<std::string, size_t>::iterator it;
        for (it = dbSizes.begin(); it != dbSizes.end(); it++)
        {
            strncpy(current_object, it->first.c_str(), sizeof(current_object));
            retcode = GenerateDatabaseFile(current_object, db_path);
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Generated %s%s.db",
                    db_path.c_str(), current_object);
            }
            else
            {
                LOG_WARN("Failed to generate %s%s.db",
                    db_path.c_str(), current_object);
            }
        }
    }

    return RTN_FAIL;
}