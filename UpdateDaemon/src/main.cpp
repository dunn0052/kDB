#include <CLI.hh>
#include <UpdateDeamon.hh>
#include <Logger.hh>
#include <unistd.h>

// prototypes
static RETCODE StartListening(const std::string& connectionAddress, const std::string& connectionPort);

int main(int argc, char* argv[])
{
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_OBJECTArgument objectArg("--object", "Object to monitor", true);
    CLI::CLI_IntArgument threadNumArg("-t", "Number of threads", true);
    CLI::CLI_IntArgument sleepArg("-s", "Time to sleep", true);
    CLI::CLI_StringArgument conenctionAddressArg("-c", "Connection address for DBUpdate", true);
    CLI::CLI_StringArgument connectionPortArg("-p", "Connection port for DBUpdate", true);


    parse.AddArg(objectArg)
        .AddArg(threadNumArg)
        .AddArg(sleepArg)
        //.AddArg(conenctionAddressArg)
        .AddArg(connectionPortArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    if(RTN_OK != retcode)
    {
        return retcode;
    }

    OBJECT_SCHEMA object_info;
    if(RTN_OK != TryGetObjectInfo(std::string(objectArg.GetValue()), object_info))
    {
        LOG_WARN("Could not open %s", objectArg.GetValue());
        return RTN_NOT_FOUND;
    }

    size_t num_threads =  4;
    if(threadNumArg.IsInUse())
    {
        num_threads = threadNumArg.GetValue();
    }

    RECORD chunk_range = object_info.numberOfRecords / num_threads;
    RECORD chunk_start = 0;

    std::vector<MonitorThread*> monitor_threads;
    for(size_t thindex = 0; thindex < num_threads; thindex++)
    {
        MonitorThread* thread = new MonitorThread();
        monitor_threads.push_back(thread);
    }

    // Will have to change this to DOFRI??
    TasQ<BASS> outgoing_changes;
    TasQ<BASS> incoming_changes;

    for(MonitorThread* monitor: monitor_threads)
    {
        monitor->Start(objectArg.GetValue(), chunk_start, chunk_start + chunk_range, &outgoing_changes, &incoming_changes);
        chunk_start += chunk_range;
    }

    // Setup read/write connection
    INETMessenger connection(connectionPortArg.GetValue());
    LOG_INFO("Accepting on %s:%s", connection.GetConnectedAddress().c_str(), connection.GetPort().c_str());
    retcode = connection.Listen();

    size_t sleep_time = sleepArg.IsInUse() ? sleepArg.GetValue() : 10;

    std::chrono::time_point start = std::chrono::steady_clock::now();
    char buffer[sizeof(BASS)];
    BASS obj;
    while(true)
    {
        // Send any updates to users
        connection.GetAcceptedConnections();
        #if 0
        while(outgoing_changes.TryPop(obj))
        {
            memcpy(buffer, &obj, sizeof(BASS));
            connection.SendToAll(buffer, sizeof(BASS));
        }
        #endif
        for(CONNECTION& conn: connection.m_Connections)
        {
            retcode = connection.Receive(conn.socket, buffer, sizeof(BASS));
            if(RTN_OK != retcode)
            {
                continue;
            }

            //incoming_changes.Push(*reinterpret_cast<BASS*>(buffer));
        }

        if(std::chrono::steady_clock::now() - start > std::chrono::seconds(sleep_time))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    long long total_recs = 0;
    for(MonitorThread* monitor: monitor_threads)
    {
        monitor->Stop();
        total_recs += monitor->m_TotalRecs;
        delete monitor;
    }

    LOG_INFO("Completed %lld record updates in %u second(s)", total_recs, sleep_time);
}

static RETCODE StartListening(const std::string& connectionAddress, const std::string& connectionPort)
{
    INETMessenger connection(std::move(connectionPort));
    //RETCODE retcode = connection.Connect(connectionAddress, connectionPort);
    LOG_INFO("Accepting on %s:%s", connection.GetConnectedAddress().c_str(), connection.GetPort().c_str());
    RETCODE retcode = connection.Listen();

# if 0
    if(RTN_OK == retcode)
    {
        LOG_INFO("Connected to %s:%s with socket %d", connection.GetConnectedAddress().c_str(), connectionPort.c_str(), connection.GetConnectionSocket());
        char buffer[1000];
        bool connected = true;
        while(connected)
        {
            retcode = connection.Receive(connection.GetConnectionSocket(), buffer, 999);
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
        LOG_WARN("Could not connect to %s:%s", connectionAddress.c_str(), connectionPort.c_str());
    }
#endif
    return retcode;
}