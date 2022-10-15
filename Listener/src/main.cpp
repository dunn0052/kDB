#include <CLI.hh>
#include <INETMessenger.hh>
#include <retcode.hh>
#include <Logger.hh>

int main(int argc, char* argv[])
{
    CLI::Parser parse("Listener", "Listen for database updates");
    CLI::CLI_StringArgument conenctionAddressArg("-c", "Connection address for Talker", true);
    CLI::CLI_StringArgument connectionPortArg("-p", "Connection port for Talker", true);
    CLI::CLI_StringArgument listenAddressArg("-l", "Listen port port (port for Listener)");
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);

    parse
        .AddArg(conenctionAddressArg)
        .AddArg(connectionPortArg)
        .AddArg(listenAddressArg)
        .AddArg(helpArg);


    RETCODE parseRetcode = parse.ParseCommandLineArguments(argc, argv);

    if(helpArg.IsInUse())
    {
        parse.Usage();
        return 0;
    }

    if(RTN_OK == parseRetcode)
    {
        std::string listenPort = "5000";
        if(listenAddressArg.IsInUse())
        {
            listenPort = listenAddressArg.GetValue();
        }

        INETMessenger connection{};
        RETCODE retcode = connection.Connect(conenctionAddressArg.GetValue(),
                           connectionPortArg.GetValue());

        if(RTN_OK == retcode)
        {
            LOG_INFO("Connected to %s:%s", connection.GetConnectedAddress().c_str(), connectionPortArg.GetValue().c_str());
        }
        else
        {
            LOG_WARN("Could not connect to %s:%s", conenctionAddressArg.GetValue().c_str(), connectionPortArg.GetValue().c_str());
        }
    }
}
