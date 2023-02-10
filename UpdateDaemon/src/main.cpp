#include <CLI.hh>
#include <DatabaseAccess.hh>
#include <UpdateDeamon.hh>
#include <Logger.hh>
#include <unistd.h>
#include <string.h>

static bool g_process_is_running = true;

static TasQ<INET_PACKAGE*> g_outgoing_changes;
static TasQ<INET_PACKAGE*> g_incoming_changes;

static void quit_signal(int sig)
{
    // strsignal is not MT-safe
    char signal_text[64];
    strcpy(signal_text, strsignal(sig));
    LOG_INFO("Quitting on signal: %s", signal_text);
    g_process_is_running = false;
}

static void client_connect(const CONNECTION& connection)
{
    LOG_INFO("Client %s:%d connected", connection.address, connection.port);
}

static void client_disconnect(const CONNECTION& connection)
{
    LOG_INFO("Client %s:%d disconnected", connection.address, connection.port);
}

// Route changes from clients to one of N incoming change queues depending on
// Which shard the change belongs to
static void ClientRequest(const INET_PACKAGE* package)
{
    LOG_DEBUG("Client %s:%d request", package->header.connection.address, package->header.connection.port);

    INET_PACKAGE* request = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_HEADER) + package->header.message_size]);
    request->header = package->header;
    memcpy(request->payload, package->payload, request->header.message_size);
    OFRI* ofri = reinterpret_cast<OFRI*>(request->payload);

    OBJECT_SCHEMA object_info;
    if(RTN_OK != TryGetObjectInfo(std::string(ofri->o), object_info))
    {
        LOG_WARN("Could not open %s -- denied", ofri->o);
        return;
    }

    if(object_info.numberOfRecords < ofri->r)
    {
        LOG_WARN("Invalid record: %lu > max: %lu", ofri->r, object_info.numberOfRecords);
        return;
    }

    g_incoming_changes.Push(request);
}


int main(int argc, char* argv[])
{
    signal(SIGQUIT, quit_signal);
    signal(SIGINT, quit_signal);
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_StringArgument portArg("-p", "Connection port for DBUpdate", true);
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);

    parse.AddArg(portArg)
        .AddArg(helpArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    if(RTN_OK != retcode || helpArg.IsInUse())
    {
        parse.Usage();
        return retcode;
    }

    MonitorThread monitor;
    monitor.Start(&g_incoming_changes, &g_outgoing_changes);

    PollThread connection(portArg.GetValue());

    LOG_INFO("Connection on %s:%s",
        connection.GetTCPAddress().c_str(),
        connection.GetTCPPort().c_str());

    connection.m_OnReceive += ClientRequest;
    connection.m_OnClientConnect += client_connect;
    connection.m_OnDisconnect += client_disconnect;

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