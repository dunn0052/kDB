#include <schema.hh>

class CLI_DatabaseArgs : public CLI::CLI_Argument<OBJECT, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, OBJECT& value)
        {
            value = conversion;
            return true;
        }
};

int main(int argc, char* argv[])
{

    CLI_DatabaseArgs databaseArgs("--object", "Name of database object", true);
    CLI::CLI_IntArgument sizeArg("-s", "New size of database object");
    CLI::Parser parser = CLI::Parser("Schema", "Verify schema, Modify databases, and Generate Header Files")
        .AddArg(sizeArg)
        .AddArg(databaseArgs);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);

    if( databaseArgs.IsInUse() )
    {
        const OBJECT objectName = databaseArgs.GetValue(0);
        RETCODE retcode = GenerateObjectDBFiles(objectName);
        if(RTN_OK != retcode)
        {
            LOG_WARN("Failed generating files for %s", objectName.c_str());
        }
    }

    if( sizeArg.IsInUse() )
    {

    }

    return 0;
}