#ifndef _PTHREAD_PROFILER_HH
#define _PTHREAD_PROFILER_HH

// View data by dragging JSON file to a chrome://tracing

// For CPP 98 -- The profiler must be initialized in the main thread or
// there is danger of the ProfPool being multi-initialized

/* Usage: Call PROFILE_FUNCTION() or PROFILE_SCOPE() to start profiling.
 * START_PROFILING() can be called to start the profiler threads without
 * logging profile metrics.

 * Notes: If program dies before ProfileWriter::EndSession() is called, the profile data will still be available,
 * but will be missing a ]} at the end of the file. Append a ]} at the end and it will fix this issue.
 * This cannot be controlled as destructors are not called upon a program being killed.
 */

#include <string>
#include <fstream>
#include <sstream>

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>  // `errno`
#include <stdint.h> // `UINT64_MAX`
#include <stdio.h>  // `printf()`
#include <string.h> // `strerror(errno)`
#include <time.h>   // `clock_gettime()` and `timespec_get()`
#include <queue>
#include <sstream>
#include <iostream>


// Program name for profile json
// Not portable outside of glibc
extern char *program_invocation_name;
extern char *program_invocation_short_name;

typedef uint64_t nanosec;

static const std::string JSON_EXT = ".json";

static inline nanosec SEC_TO_NS(time_t sec) { return (sec * 1000000000); }

// Profiling on 1; Profilling off 0
#define PROFILING 1
#if PROFILING


#define FUNCTION_SIG __PRETTY_FUNCTION__

// Macros to get function signatures in the JSON
#define PROFILE_SCOPE( NAME ) Timer timer##_LINE_( NAME )
#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNCTION_SIG)
#define START_PROFILING() ProfPool::Instance()

#else

// If profiling is set to 0, disabled, don't compile
    #define PROFILE_SCOPE( NAME )
    #define PROFILE_FUNCTION()
    #define START_PROFILING()
#endif

struct ProfileResult
{
    ProfileResult(ProfileResult& other)
        : Name(other.Name), Start(other.Start),
          End(other.End), ThreadID(other.ThreadID),
          PID(other.PID)
    {

    }

    ProfileResult& operator=(ProfileResult& other)
    {
        Name = other.Name;
        Start = other.Start;
        End = other.End;
        ThreadID = other.ThreadID;
        PID = other.PID;
        return *this;
    }

    ProfileResult(const ProfileResult& other)
    {
        Name = other.Name;
        Start = other.Start;
        End = other.End;
        ThreadID = other.ThreadID;
        PID = other.PID;
    }

    ProfileResult(const std::string& name, nanosec& start, nanosec& end,
                    pid_t& threadid, pid_t& pid)
        : Name(name), Start(start), End(end), ThreadID(threadid), PID(pid)
        {}

    ProfileResult() {}

    std::string Name;
    nanosec Start;
    nanosec End;
    pid_t ThreadID;
    pid_t PID;
};

class ProfQueue
{
    private:
        std::deque<ProfileResult> m_Tasks;
        bool m_done;
        pthread_mutex_t m_Mutex;
        pthread_cond_t m_Ready;

        ProfQueue& operator=(const ProfQueue&);

    public:
        ProfQueue() : m_Tasks{}, m_done(false), m_Mutex(PTHREAD_MUTEX_INITIALIZER)
        {
            pthread_mutex_init(&m_Mutex, NULL);
            pthread_cond_init(&m_Ready, NULL);
        }

        ProfQueue(const ProfQueue&) { }

        ~ProfQueue()
        {
            pthread_cond_destroy(&m_Ready);
            pthread_mutex_destroy(&m_Mutex);
        }

        void done()
        {
            pthread_mutex_lock(&m_Mutex);
            m_done = true;
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_broadcast(&m_Ready);
        }

        bool Pop(ProfileResult& prof)
        {
            pthread_mutex_lock(&m_Mutex);
            while(m_Tasks.empty() && !m_done)
            {
                pthread_cond_wait(&m_Ready, &m_Mutex);
            }

            if(m_Tasks.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                prof.End = 0;
                return false;
            }

            prof = m_Tasks.front();
            m_Tasks.pop_front();

            pthread_mutex_unlock(&m_Mutex);
            return true;
        }

        bool TryPop(ProfileResult& prof)
        {

            if(pthread_mutex_trylock(&m_Mutex))
            {
                prof.End = 0;
                return false;
            }

            if(m_Tasks.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                prof.End = 0;
                return false;
            }

            prof = m_Tasks.front();
            m_Tasks.pop_front();

            pthread_mutex_unlock(&m_Mutex);

            return true;
        }

        void Push(ProfileResult& prof)
        {
            pthread_mutex_lock(&m_Mutex);
            m_Tasks.push_back(prof);
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_Ready);
        }

        bool TryPush(ProfileResult& prof)
        {
            if(pthread_mutex_trylock(&m_Mutex))
            {
                return false;
            }
            m_Tasks.push_back(prof);

            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_Ready);

            return true;
        }
};

class ProfileWriter
{

private:
    int profileCount;
    bool closed;
    std::fstream m_json_stream;
    std::string m_json_filename;
    size_t writer_index;
    pthread_mutex_t m_Mutex;
    bool m_StopRequested;
    pthread_t m_ThreadID;
    bool m_Running;
    std::vector<ProfQueue>* m_ResultsQueue;


    ProfileWriter& operator=(const ProfileWriter&);

public:

    ProfileWriter(const ProfileWriter&) { }

    void execute()
    {

        ProfileResult result;
        result.End = 0;
        std::vector<ProfQueue>& queue = *m_ResultsQueue;

        m_json_stream.open(m_json_filename, std::fstream::app);

        while (!stopRequested())
        {
            // Steal tasks
            for(size_t queueIndex = 0; queueIndex != queue.size(); ++ queueIndex)
            {
                if(queue[(queueIndex + writer_index) % queue.size()].TryPop(result))
                {
                    break;
                }
            }

            // Otherwise wait for ours
            if(!result.End && !queue[writer_index].Pop(result))
            {
                // Once done, just wait for the thread to be stopped
                continue;
            }

            WriteProfile(result);
        }

        // If we stopped before our queue is empty then
        while(queue[writer_index].Pop(result))
        {
            WriteProfile(result);
        }

        m_json_stream.close();
    }

    ProfileWriter(const std::string& filepath, size_t writer_id)
        : profileCount(0), closed(false),
          m_json_filename(filepath), writer_index(writer_id),
          m_Mutex(PTHREAD_MUTEX_INITIALIZER),
          m_StopRequested(false), m_ThreadID(0), m_Running(false),
          m_ResultsQueue(nullptr)
        {
            pthread_mutex_init(&m_Mutex, NULL);
        }

    ~ProfileWriter()
    {
        if (!closed)
        {
            EndSession();
        }

        pthread_mutex_destroy(&m_Mutex);
    }

    void EndSession()
    {
        m_json_stream.close();
        profileCount = 0;
        closed = true;
        remove(m_json_filename.c_str());
    }

    void WriteProfile(const ProfileResult& result)
    {
        if(!closed)
        {
            /* next item timed */
            if (profileCount++ > 0)
            {
                m_json_stream << ",\n";
            }

            // creates timing entry to the JSON file that is read by Chrome.
            m_json_stream
                << "{\"cat\":\"function\",\"dur\":"<< (result.End - result.Start)
                << ",\"name\":\"" << result.Name
                << "\",\"ph\":\"X\",\"pid\":" << result.PID
                << ",\"tid\":" << result.ThreadID
                << ",\"ts\":" << result.Start << "}";

            /* flush here so data isn't lost in case of crash */
            m_json_stream.flush();
        }
    }

    friend std::ofstream& operator<< (std::ofstream& os, ProfileWriter& p)
    {
        std::string data;

        if(!p.m_json_stream.is_open())
        {
            p.m_json_stream.close();
        }

        if(!p.m_json_stream.good())
        {
            p.m_json_stream.clear();
        }

        p.m_json_stream.open(p.m_json_filename, std::fstream::in);

        while(getline(p.m_json_stream, data)){
            os << "\n        " << data;
        }

        p.m_json_stream.close();

        return os;
    }

        //Checks if thread is requested to stop
    bool stopRequested()
    {

        bool stopped = false;

        // If we're setting the stop request
        // It's safe to just ignore the check
        // until the next cycle
        if(pthread_mutex_trylock(&m_Mutex))
        {
            return false;
        }

        stopped = m_StopRequested;

        pthread_mutex_unlock(&m_Mutex);
        return stopped;
    }

    static void* thread_helper(void* self)
    {
        static_cast<ProfileWriter*>(self)->execute();
        return NULL;
    }

    void start(std::vector<ProfQueue>* results_queue)
    {
        m_ResultsQueue = results_queue;

        pthread_create(&m_ThreadID, NULL, ProfileWriter::thread_helper, static_cast<void*>(this));

        m_Running = true;
    }

    void stop()
    {
        if(m_Running)
        {
            m_Running = false;

            pthread_mutex_lock(&m_Mutex);
            m_StopRequested = true;
            pthread_mutex_unlock(&m_Mutex);
            pthread_join(m_ThreadID, NULL);
            m_ThreadID = 0;
        }
    }

};

class ProfPool
{
    static const size_t ROUNDS = 3;
public:

    // Could try to make this async
    void Log(ProfileResult& result)
    {
        m_PushIndex++;

        // Run through queues to see if any are available for a push
        for(size_t queueIndex = 0; queueIndex != m_Queues.size() * ROUNDS; ++queueIndex)
        {
            if(m_Queues[(m_PushIndex + queueIndex) % m_Queues.size()].TryPush(result))
            {
                return;
            }
        }

        // All queues are busy, just wait until free;
        m_Queues[m_PushIndex % m_Queues.size()].Push(result);
    }

    ProfPool(const std::string& json_profile_path = "PROCESS_PROFILE_RESULTS")
    : json_file_name(json_profile_path + JSON_EXT),
      full_json(json_file_name, std::ofstream::trunc), m_PushIndex(0),
      m_NumQueues(sysconf(_SC_NPROCESSORS_ONLN)), m_Queues(m_NumQueues)
    {
        WriteHeader();

        m_Writers.reserve(m_NumQueues); // avoid copy constructors
        std::stringstream profile_name;
        pid_t pid = getpid();
        for(size_t writer_index = 0; writer_index < m_NumQueues; writer_index++)
        {
            profile_name << "PROF_" << pid << "_" << writer_index << JSON_EXT;
            m_Writers.emplace_back(profile_name.str(), writer_index);
            profile_name.str("");
        }

        for(ProfileWriter& writer : m_Writers)
        {
            writer.start(&m_Queues);
        }
    }
    ~ProfPool()
    {

        for(ProfQueue& queue : m_Queues)
        {
            queue.done();
        }

        for(ProfileWriter& writer: m_Writers)
        {
            writer.stop();
        }

        WriteProfileData();
        WriteFooter();
    }

    void WriteHeader()
    {
        full_json << "{\"otherData\": {},\"traceEvents\":\n    [";
        full_json.flush();
    }


    void WriteProfileData()
    {
        bool first = true;

        if(!full_json.is_open())
        {
            std::cout << "full_json is not open!!\n";
        }

        for(ProfileWriter& writer: m_Writers)
        {
            if(!first)
            {
                full_json << ",";
            }
            first = false;
            full_json << writer;
            full_json.flush();
        }
    }

    void WriteFooter()
    {
        /* If the program dies before this can be called,
           "]}" can just be manually added to the end of the json file
        */

        if(!full_json.is_open())
        {
            std::cout << "full_json is not open!!\n";
        }

        full_json << "\n    ]\n}";
        full_json.close();
    }

    static ProfPool& Instance()
    {
        /* Singleton instance*/
        static ProfPool instance(std::string(program_invocation_short_name) + "_" + std::to_string(getpid()));
        return instance;
    }

private:
    ProfPool(const ProfPool&);
    ProfPool& operator=(const ProfPool&);

    private:
        std::string json_file_name;
        std::ofstream full_json;
        size_t m_PushIndex;
        size_t m_NumQueues;
        std::vector<ProfileWriter> m_Writers;
        std::vector<ProfQueue> m_Queues;
};

class Timer
{

public:
    Timer(const char* name)
        :Name(name), stopped(false), startTimepoint(now())
    {
        // Introduce timer delays so we can test optimizations
        // Delay everything except function you want to see if an
        // optimization would work.

    }

    ~Timer()
    {
        if (!stopped)
        {
            /* Stop in destructor to measure full function time */
            Stop();
        }
    }

    void Stop()
    {
        nanosec endTime = now();

        pid_t threadID = syscall(__NR_gettid);
        // This is Linux specific!!

        pid_t pid = getpid();

        ProfileResult result = ProfileResult( Name, startTimepoint, endTime, threadID, pid );

        ProfPool::Instance().Log(result);
        stopped = true;
    }

private:
    nanosec now()
    {
        nanosec nanoseconds;
        struct timespec ts;
        int return_code = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

        if (-1 == return_code)
        {
            std::cout << "Failed to obtain timestamp. errno = "
                      << errno << ": "
                      << strerror(errno)
                      << std::endl;

                      nanoseconds = UINT64_MAX; // use this to indicate error
        }
        else
        {
            nanoseconds = SEC_TO_NS(ts.tv_sec) + (nanosec)ts.tv_nsec;
        }

        return nanoseconds;
    }
private:

    const std::string Name;
    bool stopped;
    nanosec startTimepoint;
};

#endif