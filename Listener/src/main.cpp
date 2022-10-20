#include <CLI.hh>
#include <INETMessenger.hh>
#include <retcode.hh>
#include <Logger.hh>
#include <DatabaseSubscription.hh>

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
        INETMessenger connection{};
        RETCODE retcode = connection.Connect(conenctionAddressArg.GetValue(),
                           connectionPortArg.GetValue());

        if(RTN_OK == retcode)
        {
            LOG_INFO("Connected to %s:%s with socket %d", connection.GetConnectedAddress().c_str(), connectionPortArg.GetValue().c_str(), connection.GetConnectionSocket());
            char buffer[1000];
            bool connected = true;
            while(connected)
            {
                retcode = connection.Recieve(connection.GetConnectionSocket(), buffer, 999);
                if(RTN_OK != retcode)
                {
                    LOG_WARN("Connection closed!");
                    connected = false;
                    continue;
                }
                buffer[1000] = '\0';
                LOG_INFO("%s", buffer);
                memset(buffer, 0, sizeof(buffer));
                usleep(100);
            }

            LOG_INFO("Done listening!\n");
        }
        else
        {
            LOG_WARN("Could not connect to %s:%s", conenctionAddressArg.GetValue().c_str(), connectionPortArg.GetValue().c_str());
        }
    }
}
