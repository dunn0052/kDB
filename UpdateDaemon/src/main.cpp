#include <CLI.hh>
#include <UpdateDeamon.hh>
#include <Logger.hh>
#include <unistd.h>


static bool g_process_is_running = true;

static void quit_signal(int sig)
{
    g_process_is_running = false;
}

int main(int argc, char* argv[])
{
    signal(SIGQUIT, quit_signal);
    signal(SIGINT, quit_signal);
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_OBJECTArgument objectArg("--object", "Object to monitor", true);
    CLI::CLI_IntArgument threadNumArg("-t", "Number of threads", true);
    CLI::CLI_IntArgument sleepArg("-s", "Time to sleep", true);
    CLI::CLI_StringArgument portArg("-p", "Connection port for DBUpdate", true);
    CLI::CLI_FlagArgument helpArg("-h", "Shows usage", false);



    parse.AddArg(objectArg)
        .AddArg(threadNumArg)
        .AddArg(sleepArg)
        .AddArg(portArg)
        .AddArg(helpArg);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    if(RTN_OK != retcode || helpArg.IsInUse())
    {
        parse.Usage();
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
    TasQ<char*> outgoing_changes;
    TasQ<char*> incoming_changes;

    for(MonitorThread* monitor: monitor_threads)
    {
        monitor->Start(objectArg.GetValue(), chunk_start, chunk_start + chunk_range, &outgoing_changes, &incoming_changes);
        chunk_start += chunk_range;
    }

    PollThread connection(portArg.GetValue());

    size_t sleep_time = sleepArg.IsInUse() ? sleepArg.GetValue() : 10;

    std::chrono::time_point start = std::chrono::steady_clock::now();

    while(g_process_is_running)
    {

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