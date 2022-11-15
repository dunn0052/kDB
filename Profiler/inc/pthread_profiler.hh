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
#include <iostream>

// Program name for profile json
// Not portable outside of glibc
extern char *program_invocation_name;
extern char *program_invocation_short_name;

typedef uint64_t nanosec;

static const std::string JSON_EXT = ".json";

static inline nanosec SEC_TO_NS(time_t sec) { return (sec * 1000000000); }

// macro to avoid templating a function
#define SSTR( x ) static_cast< std::ostringstream & >( \
    ( std::ostringstream() << std::dec << x ) ).str()

// Profiling on 1; Profiling off 0
#define PROFILING 1
#ifdef PROFILING


// linux only!!
#define FUNCTION_SIG __PRETTY_FUNCTION__

// Printing macros
#define PROFILE_SCOPE( NAME ) Timer timer##_LINE_( NAME )
#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNCTION_SIG)
#define START_PROFILING()  ProfPool::Instance()

#else

// If _ENABLE_PROFILING isn't set then all is simply compiled out of the build
    #define PROFILE_SCOPE( NAME )
    #define PROFILE_FUNCTION()
    #define START_PROFILING()
#endif

struct ProfileResult
{
    ProfileResult(const ProfileResult& other)
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


    ProfileResult(const std::string& name, nanosec& start, nanosec& end,
                  pid_t& threadid, pid_t& pid)
        : Name(name), Start(start), End(end), ThreadID(threadid), PID(pid)
        {

        }

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
        std::queue<ProfileResult> m_Tasks;
        bool m_done;
        pthread_mutex_t m_Mutex;
        pthread_cond_t m_Ready;

        ProfQueue& operator=(const ProfQueue&);

    public:
        ProfQueue() : m_Tasks(), m_done(false), m_Mutex()
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
            m_Tasks.pop();

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
            m_Tasks.pop();

            pthread_mutex_unlock(&m_Mutex);

            return true;
        }

        void Push(ProfileResult& prof)
        {
            pthread_mutex_lock(&m_Mutex);
            m_Tasks.push(prof);
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_Ready);
        }

        bool TryPush(ProfileResult& prof)
        {
            if(pthread_mutex_trylock(&m_Mutex))
            {
                return false;
            }
            m_Tasks.push(prof);

            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_Ready);

            return true;
        }
};

class ProfileWriter
{

private:
    int m_ProfileCount;
    bool m_Closed;
    std::ofstream m_JSON_Stream;
    std::string m_json_filename;
    size_t m_Writer_Index;
    pthread_mutex_t m_Mutex;
    bool m_StopRequested;
    pthread_t m_ThreadID;
    bool m_Running;
    std::vector<ProfQueue>* m_ResultsQueue;


public:

    ProfileWriter& operator=(const ProfileWriter& other)
    {
       m_ProfileCount = other.m_ProfileCount;
       m_Closed = other.m_Closed;
       m_json_filename = other.m_json_filename;
       m_Writer_Index = other.m_Writer_Index;
       m_Mutex = other.m_Mutex;
       m_StopRequested = other.m_StopRequested;
       m_ThreadID = other.m_ThreadID;
       m_Running = other.m_Running;
       m_ResultsQueue = other.m_ResultsQueue;

       return *this;
    }

    ProfileWriter(const ProfileWriter& other)
     : m_ProfileCount(other.m_ProfileCount), m_Closed(other.m_Closed),
       m_json_filename(other.m_json_filename),
       m_Writer_Index(other.m_Writer_Index), m_Mutex(other.m_Mutex),
       m_StopRequested(other.m_StopRequested), m_ThreadID(other.m_ThreadID),
       m_Running(other.m_Running), m_ResultsQueue(other.m_ResultsQueue)
    {
        // Intentionally empty
    }

    void execute()
    {

        ProfileResult result;
        result.End = 0;
        std::vector<ProfQueue>& queue = *m_ResultsQueue;

        if(!m_JSON_Stream.is_open())
        {
            m_JSON_Stream.open(m_json_filename.c_str(), std::ofstream::trunc);
        }

        while (!stopRequested())
        {
            // Steal tasks
            for(size_t queueIndex = 0; queueIndex != queue.size(); ++ queueIndex)
            {
                if(queue[(queueIndex + m_Writer_Index) % queue.size()].TryPop(result))
                {
                    break;
                }
            }

            // Otherwise wait for ours with Pop()
            if( (0 == result.End) && (!queue[m_Writer_Index].Pop(result)) )
            {
                // Once done, just wait for the thread to be stopped
                continue;
            }

            WriteProfile(result);
        }

        // If we stopped before our queue is empty then clear it out
        while(queue[m_Writer_Index].Pop(result))
        {
            WriteProfile(result);
        }

        m_JSON_Stream.close();

        pthread_exit(NULL);

    }

    ProfileWriter(const std::string& filepath, size_t writer_id)
        : m_ProfileCount(0), m_Closed(false),
          m_json_filename(filepath), m_Writer_Index(writer_id),
          m_Mutex(),
          m_StopRequested(false), m_ThreadID(0), m_Running(false),
          m_ResultsQueue(NULL)
        {
            pthread_mutex_init(&m_Mutex, NULL);

        }

    ~ProfileWriter()
    {
        if (!m_Running)
        {
            EndSession();
        }
    }

    void EndSession()
    {
        m_ProfileCount = 0;
        m_Closed = true;
        remove(m_json_filename.c_str());
    }

    void WriteProfile(const ProfileResult& result)
    {
        if(!m_Closed)
        {
            /* next item timed */
            if (m_ProfileCount++ > 0)
            {
                m_JSON_Stream << ",\n";
            }

            // creates timing entry to the JSON file that is read by Chrome.
            m_JSON_Stream
                << "{\"cat\":\"function\",\"dur\":"<< (result.End - result.Start)
                << ",\"name\":\"" << result.Name
                << "\",\"ph\":\"X\",\"pid\":" << result.PID
                << ",\"tid\":" << result.ThreadID
                << ",\"ts\":" << result.Start << "}";

            /* flush here so data isn't lost in case of crash */
            m_JSON_Stream.flush();
        }
    }

    friend std::ofstream& operator<< (std::ofstream& os, ProfileWriter& p)
    {
        std::string data;

        if(!p.m_JSON_Stream.is_open())
        {
            p.m_JSON_Stream.close();
        }

        if(!p.m_JSON_Stream.good())
        {
            p.m_JSON_Stream.clear();
        }

        std::ifstream json;
        json.open(p.m_json_filename.c_str());
        if(!json.is_open())
        {
            std::cout << "Could not open: " << p.m_json_filename << "!\n";
            return os;
        }

        while(getline(json, data)){
            os << "\n        " << data;
        }

        json.close();

        return os;
    }

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
        if(!m_Running)
        {
            m_ResultsQueue = results_queue;

            pthread_create(&m_ThreadID, NULL, ProfileWriter::thread_helper, static_cast<void*>(this));

            m_Running = true;
        }
    }

    void stop()
    {
        if(m_Running)
        {
            m_Running = false;

            // Wait for lock here to avoid clashing with stopRequested()
            pthread_mutex_lock(&m_Mutex);
            m_StopRequested = true;
            pthread_mutex_unlock(&m_Mutex);

            // A stop has been requested so will finish up queue and quit
            pthread_join(m_ThreadID, NULL);

            pthread_mutex_destroy(&m_Mutex);

            m_ThreadID = 0;
        }
    }

};

class ProfPool
{
    // Number of times to spin around push queues
    // This value is only a guess and may be experimented on to tweak for
    // optimal performance
    static const size_t ROUNDS = 3;

public:

    // Could try to make this async
    void Log(ProfileResult& result)
    {
        // Keep moving where we start pushing to spread between queues
        m_PushIndex++; // May need to reset to avoid possible integer overflow

        // Run through queues to see if any are available for a push
        for(size_t queueIndex = 0; queueIndex != m_Queues.size() * ROUNDS; ++queueIndex)
        {
            if(m_Queues[(m_PushIndex + queueIndex) % m_Queues.size()].TryPush(result))
            {
                return;
            }
        }

        // All queues are busy, just wait until free
        m_Queues[m_PushIndex % m_Queues.size()].Push(result);
    }

    

    // A lot of work here, but only done when program exits
    ~ProfPool()
    {
        // Must be done before we stop writers
        for(size_t queue_index = 0; queue_index < m_Queues.size(); queue_index++)
        {
            m_Queues[queue_index].done();
        }

        for(size_t writer_index = 0; writer_index < m_Writers.size(); writer_index++)
        {
            m_Writers[writer_index].stop();
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

        for(size_t writer_index = 0; writer_index < m_Writers.size(); writer_index++)
        {
            ProfileWriter& writer = m_Writers[writer_index];
            if(!first)
            {
                full_json << ",";
            }

            first = false;
            full_json << writer;

            // Flush data in case of crash we have it
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
        static ProfPool instance(std::string(program_invocation_short_name) + "_" + SSTR(getpid()));
        return instance;
    }

private:

    ProfPool(const std::string& json_profile_path = "PROCESS_PROFILE_RESULTS")
    : json_file_name(json_profile_path + JSON_EXT),
      m_PushIndex(0),
      m_NumQueues(4/*sysconf(_SC_NPROCESSORS_ONLN)*/), m_Queues(m_NumQueues)
    {
        full_json.open(json_file_name.c_str(), std::ofstream::trunc);
        if(!full_json.is_open())
        {
            std::cout << "Could not open full_json! Profiling failed!\n";
            return;
        }

        WriteHeader();

        std::stringstream profile_name;
        pid_t pid = getpid();

        // Note: number of writers MUST == number of queues otherwise will run
        // into deadlock
        for(size_t writer_index = 0; writer_index < m_NumQueues; writer_index++)
        {
            profile_name << "PROF_" << pid << "_" << writer_index << JSON_EXT;
            ProfileWriter writer =ProfileWriter(profile_name.str(), writer_index);
            m_Writers.push_back(writer);
            profile_name.str("");
        }

        // Must start ONLY after all are created to avoid deadlocks
        for(size_t writer_index = 0; writer_index < m_NumQueues; writer_index++)
        {
            m_Writers[writer_index].start(&m_Queues);
        }
    }

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