#include <DBSet.hh>
#include <CLI.hh>
#include <retcode.hh>
#include <DBMap.hh>
#include <OFRI.hh>
#include <DatabaseAccess.hh>
#include <INETMessenger.hh>

static RETCODE UpdateDBValue(OFRI& ofri, const std::string& value)
{
    DatabaseAccess db_object = DatabaseAccess(ofri.o);
    return db_object.WriteValue(ofri, value);
}

static RETCODE ReadDBValue(OFRI& ofri, std::string& value)
{
    DatabaseAccess db_object = DatabaseAccess(ofri.o);
    return db_object.ReadValue(ofri, value);
}

int main(int argc, char* argv[])
{
    CLI::Parser parse("DBSet", "Update value in object");
    CLI::CLI_OBJECTArgument objectArg("-o", "Object name", true);
    CLI::CLI_IntArgument fieldArg("-f", "Field number in object", true);
    CLI::CLI_IntArgument recordArg("-r", "Record number", true);
    CLI::CLI_IntArgument indexArg("-i", "Index of field");
    CLI::CLI_StringArgument valueArg("-v", "Value update");

    parse
        .AddArg(objectArg)
        .AddArg(fieldArg)
        .AddArg(recordArg)
        .AddArg(indexArg)
        .AddArg(valueArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);
    if(IS_RETCODE_OK(retcode))
    {
        OFRI ofri = {0};
        strncpy(ofri.o, objectArg.GetValue(), sizeof(ofri.o));
        ofri.f = fieldArg.GetValue();
        ofri.r = recordArg.GetValue();
        if(indexArg.IsInUse())
        {
            ofri.i = indexArg.GetValue();
        }
        else
        {
            ofri.i = 0;
        }

        if(valueArg.IsInUse())
        {
            retcode |= UpdateDBValue(ofri, valueArg.GetValue());
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Updated ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i, " = ", valueArg.GetValue());
            }
            else
            {
                LOG_INFO("Failed to update ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i, " with ", valueArg.GetValue());
            }
        }
        else
        {
            std::string value;
            retcode |= ReadDBValue(ofri, value);
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Value of ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i, " = ", value);
            }
            else
            {
                LOG_INFO("Failed to read ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i);
            }
        }
    }

}