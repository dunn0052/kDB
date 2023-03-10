#include <schema.hh>
#include <CLI.hh>
#include <Constants.hh>
#include <ConfigValues.hh>

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

static std::string currentDirectory(char* argv[])
{
    std::string path = std::string(argv[0]);
    return path.substr(0, path.find_last_of("\\/"));
}

int main(int argc, char* argv[])
{
    CLI_DatabaseArgs databaseArgs("--object", "Name of database object");
    CLI::CLI_StringArgument inc_path("-h", "Path to write object header file");
    CLI::CLI_StringArgument skm_path("-s", "Path to write object schema file");
    CLI::CLI_StringArgument py_path("-p", "Path to write object Python file");
    CLI::CLI_FlagArgument all_arg("-a", "Build all schema");
    CLI::Parser parser = CLI::Parser("Schema",
        "Verify schema, Modify databases, and Generate Header Files")
        .AddArg(inc_path)
        .AddArg(skm_path)
        .AddArg(py_path)
        .AddArg(all_arg)
        .AddArg(databaseArgs);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(!IS_RETCODE_OK(retcode))
    {
        parser.Usage();
        return retcode;
    }

    std::string INSTALL_DIR = ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        LOG_ERROR("Could not find ", KDB_INSTALL_DIR, " in environment!");
        return RTN_NOT_FOUND;
    }

    const std::string db_schema_path = skm_path.IsInUse() ?
        skm_path.GetValue() : INSTALL_DIR + DB_SKM_DIR;

    const std::string db_header_path = inc_path.IsInUse() ?
        inc_path.GetValue() : INSTALL_DIR + DB_INC_DIR;

    const std::string db_py_path = py_path.IsInUse() ?
        py_path.GetValue() : INSTALL_DIR + DB_PY_DIR;

    if( databaseArgs.IsInUse() )
    {
        const OBJECT& objectName = databaseArgs.GetValue();
        #if 0
        retcode |= GenerateObjectDBFiles(objectName,
            db_schema_path,
            db_header_path);
        #endif
        if(!IS_RETCODE_OK(retcode))
        {
            LOG_WARN("Failed generating files for ", objectName);
        }
    }
    else if(all_arg.IsInUse())
    {
        retcode |= GenerateAllDBFiles(
            db_schema_path, db_header_path, db_py_path);

        if(!IS_RETCODE_OK(retcode))
        {
            LOG_WARN("Failed generating files!");
        }
    }

    return retcode;
}