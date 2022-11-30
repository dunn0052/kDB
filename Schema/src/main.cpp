#include <schema.hh>
#include <CLI.hh>
#include <Constants.hh>

class CLI_DatabaseArgs : public CLI::CLI_Argument<OBJECT, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, OBJECT& value)
        {
            if(OBJECT_NAME_LEN >= conversion.length())
            {
                memcpy(value, conversion.c_str(), sizeof(value));
                return true;
            }

            return false;
        }
};

int main(int argc, char* argv[])
{
    CLI_DatabaseArgs databaseArgs("--object", "Name of database object", true);
    CLI::CLI_StringArgument inc_path("-h", "Path to write object header file");
    CLI::CLI_StringArgument skm_path("-s", "Path to write object schema file");
    CLI::CLI_FlagArgument all_arg("-a", "Build all schema");
    CLI::Parser parser = CLI::Parser("Schema",
        "Verify schema, Modify databases, and Generate Header Files")
        .AddArg(inc_path)
        .AddArg(skm_path)
        .AddArg(all_arg)
        .AddArg(databaseArgs);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);

    if( databaseArgs.IsInUse() )
    {
        const std::string db_schema_path = skm_path.IsInUse() ?
            skm_path.GetValue() : DB_SKM_DIR;

        const std::string db_header_path = inc_path.IsInUse() ?
            inc_path.GetValue() : DB_INC_DIR;

        const OBJECT& objectName = databaseArgs.GetValue();

        RETCODE retcode = GenerateObjectDBFiles(objectName,
            db_schema_path,
            db_header_path);

        if(!IS_RETCODE_OK(retcode))
        {
            LOG_WARN("Failed generating files for %s", objectName);
        }
    }

    return RTN_OK;
}