#include <DBSet.hh>
#include <CLI.hh>
#include <retcode.hh>
#include <DBMap.hh>
#include <DOFRI.hh>
#include <DatabaseAccess.hh>

static RETCODE UpdateDBValue(DOFRI& dofri, const std::string& value)
{
    DatabaseAccess db_object = DatabaseAccess(dofri.o);
    return db_object.WriteValue(dofri, value);
}

static RETCODE ReadDBValue(DOFRI& dofri, std::string& value)
{
    DatabaseAccess db_object = DatabaseAccess(dofri.o);
    return db_object.ReadValue(dofri, value);
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
        DOFRI dofri = {0};
        strncpy(dofri.o, objectArg.GetValue(), sizeof(dofri.o));
        dofri.f = fieldArg.GetValue();
        dofri.r = recordArg.GetValue();
        if(indexArg.IsInUse())
        {
            dofri.i = indexArg.GetValue();
        }
        else
        {
            dofri.i = 0;
        }

        if(valueArg.IsInUse())
        {
            retcode |= UpdateDBValue(dofri, valueArg.GetValue());
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Updated %s.%u.%u.%u = %s",
                    dofri.o, dofri.f, dofri.r, dofri.i, valueArg.GetValue().c_str());
            }
            else
            {
                LOG_WARN("Failed to update %s.%u.%u.%u with %s",
                    dofri.o, dofri.f, dofri.r, dofri.i, valueArg.GetValue().c_str());
            }
        }
        else
        {
            std::string value;
            retcode |= ReadDBValue(dofri, value);
            if(IS_RETCODE_OK(retcode))
            {
                LOG_INFO("Value of %s.%u.%u.%u = %s",
                    dofri.o, dofri.f, dofri.r, dofri.i, value.c_str());
            }
            else
            {
                LOG_WARN("Failed to read %s.%u.%u.%u",
                    dofri.o, dofri.f, dofri.r, dofri.i);
            }
        }
    }

}