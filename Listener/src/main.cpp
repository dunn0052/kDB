#include <CLI.hh>
#include <INETMessenger.hh>
#include <retcode.hh>
#include <Logger.hh>
#include <TasQ.hh>
#include <MessageTypes.hh>

static bool running = true;

static void quitSignal(int sig)
{
    LOG_INFO("Signal ", sig, " caught!");
    running = false;
}

static void StopListening(int _)
{
    LOG_INFO("Stopping Listener");
}

void PrintClientConnect(const CONNECTION& connection)
{
    LOG_INFO("Client ", connection.address, ":", connection.port, " connected" );
}

void PrintServerConnect(const CONNECTION& connection)
{
    LOG_INFO("Server connected to ", connection.address, ":", connection.port);
}

void PrintDisconnect(const CONNECTION& connection)
{
    LOG_INFO("Client ", connection.address, ":", connection.port, " disconnected" );
}

void PrintMessage(const INET_PACKAGE* package)
{
    switch(package->header.data_type)
    {
        case MESSAGE_TYPE::TEXT:
        {
            LOG_INFO("Connection ", package->header.connection.address, ":", package->header.connection.port, " sent a message: ", package->payload);
            break;
        }
        default:
        {
            LOG_INFO("Connection ", package->header.connection.address, ":", package->header.connection.port, " sent a package");
            break;
        }
    }
}

class WriteThread: public DaemonThread<TasQ<INET_PACKAGE*>*>
{
    void execute(TasQ<INET_PACKAGE*>* p_queue)
    {
        std::string user_input;
        INET_PACKAGE* message = nullptr;
        RETCODE retcode = RTN_OK;

        TasQ<INET_PACKAGE*>& messages = *p_queue;

        while(StopRequested() == false)
        {
            std::getline(std::cin, user_input);

            if(user_input.rfind("ofri", 0) == 0)
            {
                std::cout << "Enter ofri: \n";
                std::getline(std::cin, user_input);
                std::stringstream ofri_input;
                ofri_input << user_input;

                LOG_INFO("OFRI: ", ofri_input.str());
                OFRI ofri = {0};
                if(ofri_input >> ofri.o >> ofri.r >> ofri.f >> ofri.i)
                {
                    message = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + sizeof(OFRI)]);
                    message->header.message_size = sizeof(OFRI);
                    message->header.data_type = MESSAGE_TYPE::DB_WRITE;
                    memcpy(message->payload, &ofri, sizeof(OFRI));
                }
                else
                {
                    LOG_WARN("Failed to read ofri: ", user_input);
                    continue;
                }
            }
            else
            {
                message = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + user_input.length() + 1]);
                message->header.message_size = user_input.length() + 1;
                message->header.data_type = MESSAGE_TYPE::TEXT;
                strncpy(message->payload, user_input.c_str(), user_input.length() + 1);
            }
            messages.Push(message);
        }
    }

};

int main(int argc, char* argv[])
{
    signal(SIGQUIT, quitSignal);
    signal(SIGINT, quitSignal);
    CLI::Parser parse("Listener", "Listen for database updates");
    CLI::CLI_StringArgument connectionAddressArg("-c", "Connection address for Other", false);
    CLI::CLI_StringArgument connectionPortArg("-p", "Connection port for Other", false);
    CLI::CLI_StringArgument listeningPortArg("-l", "Listening port", true);
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);

    parse
        .AddArg(connectionAddressArg)
        .AddArg(connectionPortArg)
        .AddArg(listeningPortArg)
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
        LOG_INFO("Listening for connections on: ", connection.GetTCPAddress(), ":", connection.GetTCPPort());

        // Connection if requested
        if(connectionAddressArg.IsInUse() && connectionPortArg.IsInUse())
        {
            // If we're the initiator then try connect
            retcode = connection.Connect(connectionAddressArg.GetValue(),
                connectionPortArg.GetValue());
            if(RTN_OK != retcode)
            {
                LOG_WARN("Couldn't connect to ",
                    connection.GetTCPAddress(), ":", connectionPortArg.GetValue());
            }
            LOG_INFO("Connected to ", connection.GetTCPAddress(),  ":", connectionPortArg.GetValue(), " with socket: ",
                 connection.GetTCPSocket());

        }

        connection.m_OnClientConnect += PrintClientConnect;
        connection.m_OnServerConnect += PrintServerConnect;
        connection.m_OnDisconnect += PrintDisconnect;
        connection.m_OnReceive += PrintMessage;
        connection.m_OnStop += StopListening;

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
                    connection.SendAll(message);
                }

                usleep(100);
            }

            connection.StopPoll();

            LOG_INFO("Done listening!\n");
        }
        else
        {
            LOG_WARN("Failed -- exiting!");
        }

        return retcode;
    }
    else
    {
        parse.Usage();
    }

    return parseRetcode;
}
