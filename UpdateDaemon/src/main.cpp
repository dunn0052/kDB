#include <CLI.hh>
#include <DatabaseAccess.hh>
#include <UpdateDeamon.hh>
#include <Logger.hh>
#include <unistd.h>
#include <string.h>

static bool g_process_is_running = true;

static TasQ<INET_PACKAGE*> g_outgoing_changes;
static TasQ<INET_PACKAGE*> g_incoming_changes;

static void quitSignal(int sig)
{
    // strsignal is not MT-safe
    char signal_text[64];
    strcpy(signal_text, strsignal(sig));
    LOG_INFO("Quitting on signal: ", signal_text);
    g_process_is_running = false;
}

static void clientConnect(const CONNECTION& connection)
{
    LOG_INFO("Client ", connection.address, ":", connection.port, " connected" );
}

static void clientDisconnect(const CONNECTION& connection)
{
    LOG_INFO("Client ", connection.address, ":", connection.port, " disconnected" );
}

// Route changes from clients to one of N incoming change queues depending on
// Which shard the change belongs to
static void ClientRequest(const INET_PACKAGE* package)
{
    LOG_DEBUG("Client ", package->header.connection.address, ":", package->header.connection.port, " request");

    INET_PACKAGE* request = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_HEADER) + package->header.message_size]);
    request->header = package->header;
    memcpy(request->payload, package->payload, request->header.message_size);
    OFRI* ofri = reinterpret_cast<OFRI*>(request->payload);

    OBJECT_SCHEMA object_info;
    if(RTN_OK != TryGetObjectInfo(std::string(ofri->o), object_info))
    {
        LOG_WARN("Could not open: ", ofri->o);
        return;
    }

    if(object_info.numberOfRecords < ofri->r)
    {
        LOG_WARN("Invalid record: ", ofri->r, " > max: ", object_info.numberOfRecords);
        return;
    }

    g_incoming_changes.Push(request);
}

class TestRecv
{
    public:
    void TesetClassRecv(const INET_PACKAGE* package)
    {
        LOG_DEBUG("Client ", package->header.connection.address, ":", package->header.connection.port, " request");

        INET_PACKAGE* request = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_HEADER) + package->header.message_size]);
        request->header = package->header;
        memcpy(request->payload, package->payload, request->header.message_size);
        OFRI* ofri = reinterpret_cast<OFRI*>(request->payload);

        OBJECT_SCHEMA object_info;
        if(RTN_OK != TryGetObjectInfo(std::string(ofri->o), object_info))
        {
            LOG_WARN("Could not open: ", ofri->o);
            return;
        }

        if(object_info.numberOfRecords < ofri->r)
        {
            LOG_WARN("Invalid record: ", ofri->r, " > max: ", object_info.numberOfRecords);
            return;
        }

    }
};

int main(int argc, char* argv[])
{
    signal(SIGQUIT, quitSignal);
    signal(SIGINT, quitSignal);
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_StringArgument portArg("-p", "Connection port for DBUpdate");
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);

    parse.AddArg(portArg)
        .AddArg(helpArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    if(RTN_OK != retcode || helpArg.IsInUse())
    {
        parse.Usage();
        return retcode;
    }

    std::string port = portArg.IsInUse() ?
        portArg.GetValue() : ConfigValues::Instance().Get(KDB_INET_PORT);

    MonitorThread monitor;
    monitor.Start(&g_incoming_changes, &g_outgoing_changes);

    PollThread connection(portArg.GetValue());

    LOG_INFO("Connection on ", connection.GetTCPAddress(), ":", connection.GetTCPPort());

    TestRecv test;

    connection.m_OnReceive += [&](const INET_PACKAGE* package){ test.TesetClassRecv(package); };
    connection.m_OnClientConnect += clientConnect;
    connection.m_OnDisconnect += clientDisconnect;

    std::chrono::time_point start = std::chrono::steady_clock::now();

    INET_PACKAGE* outgoing_message = nullptr;
    while(g_process_is_running)
    {
        while(g_outgoing_changes.TryPop(outgoing_message))
        {
            connection.SendAll(outgoing_message);
            delete outgoing_message;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    monitor.Stop();
}