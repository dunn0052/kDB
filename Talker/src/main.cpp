#include <Database.hh>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <TEST.hh>

#include <CLI.hh>
#include <INETMessenger.hh>

class CLI_ObjectArgs : public CLI::CLI_Argument<OBJECT, 1, 1>
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
    CLI::Parser parse("Database Mapper", "Read mapped objects");
    CLI_ObjectArgs objArg("--object", "Name of object", true);
    CLI::CLI_IntArgument recordArg("-r", "Object record", true);
    parse
        .AddArg(objArg)
        .AddArg(recordArg);

    parse.ParseCommandLineArguments(argc, argv);

    RETCODE retcode = RTN_OK;

    Database test = Database();
    int record = 0;

    if( objArg.IsInUse() && recordArg.IsInUse() )
    {
        retcode |= test.Open(objArg.GetValue(0));
        record = recordArg.GetValue(0);
    }
    else
    {
        return 1;
    }

    if(RTN_OK == retcode )
    {
        std::cout << "Opened test successfully!\n";
    }
    else
    {
        std::cout << "Failed to open database: " << objArg.GetValue(0) << "! Exiting.\n";
        return 1;
    }

    TEST* testobj = test.Get<TEST>(record);
    if(nullptr != testobj)
    {
        strcpy(testobj->NAME ,"test");
        LOG_INFO("TEST[%d]->NAME: %s", record,  testobj->NAME);
    }
    else
    {
        std::cout << "Failed to get TEST object!\n";
        return 1;
    }

    INETMessenger comms("1234");
    retcode |= comms.Listen();

    return retcode;
}