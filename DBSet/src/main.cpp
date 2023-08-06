#include <DBSet.hh>
#include <CLI.hh>
#include <retcode.hh>
#include <DBMap.hh>
#include <OFRI.hh>
#include <DatabaseAccess.hh>
#include <INETMessenger.hh>

#include <pthread_profiler.hh>

static RETCODE UpdateDBValue(OFRI& ofri, const std::string& value)
{
    PROFILE_FUNCTION();
    DatabaseAccess db_object = DatabaseAccess(ofri.o);
    return db_object.WriteValue(ofri, value);
}

static RETCODE ReadDBValue(OFRI& ofri, std::string& value)
{
    PROFILE_FUNCTION();
    DatabaseAccess db_object = DatabaseAccess(ofri.o);
    return db_object.ReadValue(ofri, value);
}

int main(int argc, char* argv[])
{
    PROFILE_FUNCTION();
    CLI::Parser parse("DBSet", "Update value in object.");
    CLI::CLI_OFRIArgument ofriArg("--ofri", "OBJECT.0.0.0", true);
    CLI::CLI_StringArgument valueArg("=", "Value update");
    CLI::CLI_ORArgument orArg("--or", "OBJECT.0");

    parse
        .AddArg(valueArg)
        .AddArg(ofriArg)
        .AddArg(orArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);
    if(IS_RETCODE_OK(retcode))
    {
        OFRI ofri = ofriArg.GetValue();

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
    else
    {
        parse.Usage();
        retcode |= RTN_FAIL;
    }

    return retcode;
}