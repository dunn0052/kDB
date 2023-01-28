#include <CLI.hh>
#include <INETMessenger.hh>
#include <retcode.hh>
#include <Logger.hh>
#include <DatabaseSubscription.hh>
#include <TasQ.hh>

static bool running = true;

static void quit_signal(int sig)
{
    LOG_INFO("Signal %d caught!", sig);
    LOG_INFO("Ending Listener");
    running = false;
}

void PrintClientConnect(const CONNECTION& connection)
{
    LOG_INFO("Client connected %s:%d", connection.address, connection.port);
}

void PrintServerConnect(const CONNECTION& connection)
{
    LOG_INFO("Server connected to %s:%d", connection.address, connection.port);
}

void PrintDisconnect(const CONNECTION& connection)
{
    LOG_INFO("Connection disconnected %s:%d", connection.address, connection.port);
}

void PrintMessage(const CONNECTION& connection, const char* message)
{
    LOG_INFO("Connection %s:%d send a message: %s", connection.address, connection.port, message);
}

class WriteThread: public DaemonThread<TasQ<INET_PACKAGE*>*>
{
    void execute(TasQ<INET_PACKAGE*>* p_queue)
    {
        std::string user_input;
        RETCODE retcode = RTN_OK;

        TasQ<INET_PACKAGE*>& messages = *p_queue;

        while(StopRequested() == false)
        {
            std::cin >> user_input;
            INET_PACKAGE* message = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + user_input.length() + 1]);
            message->header.message_size = user_input.length() + 1;
            strncpy(message->payload, user_input.c_str(), user_input.length() + 1);
            messages.Push(message);
        }
    }

};

int main(int argc, char* argv[])
{
    signal(SIGQUIT, quit_signal);
    signal(SIGINT, quit_signal);
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

    if(RTN_OK == parseRetcode)
    {
        RETCODE retcode = RTN_OK;
        PollThread connection(listeningPortArg.GetValue());

        if(RTN_OK != retcode)
        {
            LOG_WARN("Failed to start listening!\nExiting\n");
            return retcode;
        }
        LOG_INFO("Listening for connections on: %s:%s", connection.GetTCPAddress().c_str(), connection.GetTCPPort().c_str());

        // Connection if requested
        if(connectionAddressArg.IsInUse() && connectionPortArg.IsInUse())
        {
            // If we're the initiator then try connect
            retcode = connection.Connect(connectionAddressArg.GetValue(),
                connectionPortArg.GetValue());
            if(RTN_OK != retcode)
            {
                LOG_WARN("Couldn't connect to %s:%s\nExiting...",
                    connection.GetTCPAddress().c_str(), connectionPortArg.GetValue().c_str());
            }
            LOG_INFO("Connected to %s:%s with socket %d",
                connection.GetTCPAddress().c_str(), connectionPortArg.GetValue().c_str(), connection.GetTCPSocket());

        }

        connection.m_OnClientConnect += PrintClientConnect;
        connection.m_OnServerConnect += PrintServerConnect;
        connection.m_OnDisconnect += PrintDisconnect;
        connection.m_OnReceive += PrintMessage;

        TasQ<INET_PACKAGE*> messages;
        WriteThread* writer_thread = new WriteThread();
        writer_thread->Start(&messages);


        if(RTN_OK == retcode)
        {
            INET_PACKAGE* message;
            RETCODE retcode = RTN_OK;
            while(running)
            {
                while(messages.TryPop(message))
                {
                    //strncpy(message->header.connection_token.address, connectionAddressArg.GetValue().c_str(), sizeof(message->header.connection_token.address));
                    connection.SendAll(message);
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
