#include <CLI.hh>
#include <INETMessenger.hh>
#include <retcode.hh>
#include <Logger.hh>
#include <DatabaseSubscription.hh>
#include <TasQ.hh>

class WriteThread: public DaemonThread<TasQ<std::string>*>
{
    void execute(TasQ<std::string>* p_queue)
    {
        std::string user_input;
        RETCODE retcode = RTN_OK;

        TasQ<std::string>& messages = *p_queue;

        while(StopRequested() == false)
        {
            std::cin >> user_input;
            messages.Push(user_input);
        }
    }

};

int main(int argc, char* argv[])
{
    CLI::Parser parse("Listener", "Listen for database updates");
    CLI::CLI_StringArgument connectionAddressArg("-c", "Connection address for Other", false);
    CLI::CLI_StringArgument connectionPortArg("-p", "Connection port for Other", false);
    CLI::CLI_StringArgument listeningPortArg("-l", "Listening port", true);
    CLI::CLI_IntArgument waitListenArg("-w", "Time to wait for sending messages", true);
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);

    parse
        .AddArg(connectionAddressArg)
        .AddArg(connectionPortArg)
        .AddArg(listeningPortArg)
        .AddArg(waitListenArg)
        .AddArg(helpArg);


    RETCODE parseRetcode = parse.ParseCommandLineArguments(argc, argv);

    if(helpArg.IsInUse())
    {
        parse.Usage();
        return 0;
    }

    int connected_socket = -1;

    if(RTN_OK == parseRetcode)
    {
        RETCODE retcode = RTN_OK;
        INETMessenger connection(listeningPortArg.GetValue());

        retcode = connection.Listen();
        if(RTN_OK != retcode)
        {
            LOG_WARN("Failed to start listening!\nExiting\n");
            return retcode;
        }
        LOG_INFO("Listening for connections on: %s:%s", connection.GetAddress().c_str(), connection.GetPort().c_str());

        if(connectionAddressArg.IsInUse() && connectionPortArg.IsInUse())
        {
            // If we're the initiator then try connect
            retcode = connection.Connect(connectionAddressArg.GetValue(),
                connectionPortArg.GetValue());
            if(RTN_OK != retcode)
            {
                LOG_WARN("Couldn't connect to %s:%s\nExiting...",
                    connection.GetConnectedAddress().c_str(), connectionPortArg.GetValue().c_str());
            }
            LOG_INFO("Connected to %s:%s with socket %d",
                connection.GetConnectedAddress().c_str(), connectionPortArg.GetValue().c_str(), connection.GetConnectionSocket());

        }

        TasQ<std::string> messages;
        WriteThread* writer_thread = new WriteThread();
        writer_thread->Start(&messages);

        if(RTN_OK == retcode)
        {
            char buffer[1000] = {0};
            bool connected = true;
            std::string user_input;
            RETCODE retcode = RTN_OK;

            auto start_time = std::chrono::system_clock::now();

            std::vector<size_t> bad_connections;
            while(connected)
            {
                for(CONNECTION& con : connection.m_Connections)
                {
                    if(con.socket != 0)
                    {
                        retcode = connection.Receive(con.socket, buffer, 999);
                    }
#if 0
                    if(RTN_CONNECTION_FAIL == retcode)
                    {
                        LOG_WARN("Connection %s:%d closed!", con.address, con.socket);
                        //connected = false;
                        //con.socket = 0;
                        //retcode = RTN_OK;
                        //continue;
                    }
#endif
                    if(RTN_OK == retcode)
                    {
                        buffer[1000] = '\0';
                        LOG_INFO("Other: %s", buffer);
                        memset(buffer, 0, sizeof(buffer));
                    }
                }

                while(messages.TryPop(user_input))
                {
                    connection.SendToAll(user_input);
                }

                // Poll every 5 seconds
                if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count() > 5)
                {
                    connection.GetAcceptedConnections();
                    start_time = std::chrono::high_resolution_clock::now();
                }

                usleep(100);
            }

            LOG_INFO("Done listening!\n");
        }
        else
        {
            LOG_WARN("Could not connect to %s:%s", connectionAddressArg.GetValue().c_str(), connectionPortArg.GetValue().c_str());
        }
    }
}
