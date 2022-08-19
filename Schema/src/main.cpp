#include <schema.hh>

class CLI_DatabaseArgs : public CLI::CLI_Argument<DATABASE, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, DATABASE& value)
        {
            value = conversion;
            return true;
        }
};

int main(int argc, char* argv[])
{

    CLI_DatabaseArgs databaseArgs("--database", "Name of database", true);
    CLI::CLI_IntArgument sizeArg("-s", "New size of database");
    CLI::Parser parser = CLI::Parser("Schema", "Verify schema, Modify databases, and Generate Header Files")
        .AddArg(sizeArg)
        .AddArg(databaseArgs);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);

    if( databaseArgs.IsInUse() )
    {
        const DATABASE databaseName = databaseArgs.GetValue(0);
        RETCODE retcode = GenerateDatabase(databaseName);
    }

    if( sizeArg.IsInUse() )
    {

    }

    return 0;
}