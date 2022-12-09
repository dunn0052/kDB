#include <CLI.hh>
#include <UpdateDeamon.hh>
#include <unistd.h>

int main(int argc, char* argv[])
{
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_OBJECTArgument objectArg("--object", "Object to monitor", true);
    CLI::CLI_IntArgument threadNumArg("-t", "Number of threads");
    CLI::CLI_IntArgument sleepArg("-s", "Time to sleep");

    parse.AddArg(objectArg).AddArg(threadNumArg).AddArg(sleepArg);

    parse.ParseCommandLineArguments(argc, argv);

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

    std::vector<MonitorThread> monitor_threads;
    for(size_t thindex = 0; thindex < num_threads; thindex++)
    {
        monitor_threads.push_back(MonitorThread());
    }

    for(MonitorThread& monitor: monitor_threads)
    {
        monitor.Start(objectArg.GetValue(), chunk_start, chunk_start + chunk_range);
        chunk_start += chunk_range;
    }

    size_t sleep_time = sleepArg.IsInUse() ? sleepArg.GetValue() : 10;
    /* doing "work" */
    sleep(sleep_time);

    long long total_recs = 0;
    for(MonitorThread& monitor: monitor_threads)
    {
        monitor.Stop();
        total_recs += monitor.m_TotalRecs;
    }

    std::cout << "Completed " << total_recs << " record updates in " << sleep_time << " second(s)!\n";
}