#include <CLI.hh>
#include <DOFRI.hh>
#include <allDBs.hh>
#include <DBSizer.hh>
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

static RETCODE GenerateDatabaseFile(const OBJECT& object_name, const size_t number_of_records, const std::string& dbPath)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << INSTALL_DIR << dbPath <<  object_name << DB_EXT;
    const std::string& path = filepath.str();
    size_t fileSize = 0;

    RETURN_RETCODE_IF_NOT_OK(ObjectSize(object_name, fileSize));

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
    CLI_CharArrayTest test("-c", "Test of a 20 char array");
    CLI::CLI_StringArgument str("-s", "Test string!");

    parse.AddArg(test).AddArg(str);

    parse.ParseCommandLineArguments(argc, argv);

    if(test.IsInUse())
    {
        std::cout << "Worked with value of: " << test.GetValue() << std::endl;
    }

    if(str.IsInUse())
    {
        std::cout << "Worked with value of: " << str.GetValue() << std::endl;
    }
}