//#include <Database.hh>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <string>
//#include <TEST.hh>

#include <CLI.hh>
#include <INETMessenger.hh>

class CLI_ObjectArgs : public CLI::CLI_Argument<OBJECT, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, OBJECT& value)
        {
            if(OBJECT_NAME_LEN <= conversion.length())
            {
                memcpy(value, conversion.c_str(), sizeof(value));
                return true;
            }

            return false;
        }
};

int main(int argc, char* argv[])
{
    CLI::Parser parse("Talker", "Send DB Updates");
    CLI::CLI_StringArgument portArg("-p", "Port to connect to Talker (this process)", true);
    CLI::CLI_IntArgument waitTimeArg("-w", "How long to wait for a connection", true);

    parse
        .AddArg(waitTimeArg)
        .AddArg(portArg);

    RETCODE parseRetcode = parse.ParseCommandLineArguments(argc, argv);

#if 0
    if(RTN_OK == parseRetcode)
    {
        INETMessenger comms(portArg.GetValue());
        LOG_INFO("Accepting on %s:%s", comms.GetAddress().c_str(), comms.GetPort().c_str());
        parseRetcode |= comms.Listen();

        std::string user_input;
        for(int i = waitTimeArg.GetValue(); i > 0; i--)
        {
            comms.GetAcceptedConnections();
            std::cout << "Enter message: ";
            std::cin >> user_input;
            comms.SendToAll(user_input);
        }

        return RTN_OK;
    }

#endif
    return RTN_FAIL;
}