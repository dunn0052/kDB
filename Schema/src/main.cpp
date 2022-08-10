#include <schema.hh>

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
        std::ifstream database;
        const DATABASE databaseName = databaseArgs.GetValue(0);
        RETCODE retcode = LoadDatabase(databaseName, database);
    }

    if( sizeArg.IsInUse() )
    {

    }

    return 0;
}