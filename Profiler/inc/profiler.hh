#ifndef _PROFILER_HH
#define _PROFILER_HH

// View data by dragging JSON file to a chrome://tracing

/* Usage: Call PROFILE_FUNCTION() or PROFILE_SCOPE() to start profiling.
 * START_PROFILING() can be called to start the profiler threads without
 * logging profile metrics

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

#include <DaemonThread.hh>

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
    ProfileResult(ProfileResult&& other)
        : m_Name(std::move(other.m_Name)), m_Start(std::move(other.m_Start)),
          m_End(std::move(other.m_End)), m_ThreadID(std::move(other.m_ThreadID)),
          m_PID(std::move(other.m_PID))
    {

    }

    ProfileResult& operator=(ProfileResult&& other)
    {
        m_Name = std::move(other.m_Name);
        m_Start = std::move(other.m_Start);
        m_End = std::move(other.m_End);
        m_ThreadID = std::move(other.m_ThreadID);
        m_PID = std::move(other.m_PID);
        return *this;
    }

    ProfileResult(const ProfileResult& other)
    {
        m_Name = other.m_Name;
        m_Start = other.m_Start;
        m_End = other.m_End;
        m_ThreadID = other.m_ThreadID;
        m_PID = other.m_PID;
    }

    ProfileResult(const std::string& name, nanosec& start, nanosec& end,
                    pid_t& threadid, pid_t& pid)
        : m_Name(name), m_Start(start), m_End(end), m_ThreadID(threadid), m_PID(pid)
        {}

    ProfileResult() {}

    std::string m_Name;
    nanosec m_Start;
    nanosec m_End;
    pid_t m_ThreadID;
    pid_t m_PID;
};

class ProfQueue
{
    private:
        std::deque<ProfileResult> m_Tasks;
        bool m_Done;
        pthread_mutex_t m_Mutex;
        pthread_cond_t m_ReadyCondition;

        ProfQueue& operator=(const ProfQueue&);

    public:
        ProfQueue() : m_Tasks{}, m_Done(false), m_Mutex(PTHREAD_MUTEX_INITIALIZER)
        {
            pthread_mutex_init(&m_Mutex, NULL);
            pthread_cond_init(&m_ReadyCondition, NULL);
        }

        ProfQueue(const ProfQueue&) { }

        ~ProfQueue()
        {
            pthread_cond_destroy(&m_ReadyCondition);
            pthread_mutex_destroy(&m_Mutex);
        }

        void done()
        {
            pthread_mutex_lock(&m_Mutex);
            m_Done = true;
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_broadcast(&m_ReadyCondition);
        }

        bool Pop(ProfileResult& prof)
        {
            pthread_mutex_lock(&m_Mutex);
            while(m_Tasks.empty() && !m_Done)
            {
                pthread_cond_wait(&m_ReadyCondition, &m_Mutex);
            }

            if(m_Tasks.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                prof.m_End = 0;
                return false;
            }

            prof = std::move(m_Tasks.front());
            m_Tasks.pop_front();

            pthread_mutex_unlock(&m_Mutex);
            return true;
        }

        bool TryPop(ProfileResult& prof)
        {

            if(pthread_mutex_trylock(&m_Mutex))
            {
                prof.m_End = 0;
                return false;
            }

            if(m_Tasks.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                prof.m_End = 0;
                return false;
            }

            prof = std::move(m_Tasks.front());
            m_Tasks.pop_front();

            pthread_mutex_unlock(&m_Mutex);

            return true;
        }

        void Push(ProfileResult&& prof)
        {
            pthread_mutex_lock(&m_Mutex);
            m_Tasks.push_back(std::move(prof));
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_ReadyCondition);
        }

        bool TryPush(ProfileResult&& prof)
        {
            if(pthread_mutex_trylock(&m_Mutex))
            {
                return false;
            }
            m_Tasks.push_back(std::move(prof));

            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_ReadyCondition);

            return true;
        }
};

class ProfileWriter : public DaemonThread<std::vector<ProfQueue>*>
{

private:
    int m_ProfileCount;
    bool closed;
    std::fstream m_json_stream;
    std::string m_json_filename;
    int writer_index;


    ProfileWriter& operator=(const ProfileWriter&);

public:

    ProfileWriter(const ProfileWriter&) { }

    void execute(std::vector<ProfQueue>* results_queue)
    {

        ProfileResult result;
        result.m_End = 0;
        std::vector<ProfQueue>& queue = *results_queue;

        m_json_stream.open(m_json_filename, std::fstream::app);

        while (!StopRequested())
        {
            // Steal tasks
            for(size_t queueIndex = 0; queueIndex != results_queue->size(); ++ queueIndex)
            {
                if(queue[(queueIndex + writer_index) % results_queue->size()].TryPop(result))
                {
                    break;
                }
            }

            // Otherwise wait for ours
            if(!result.m_End && !queue[writer_index].Pop(result))
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

    ProfileWriter(const std::string& filepath, int writer_id)
        : m_ProfileCount(0), closed(false),
          m_json_filename(filepath), writer_index(writer_id)
        {
        }

    ~ProfileWriter()
    {
        if (!closed)
        {
            EndSession();
        }
    }

    void EndSession()
    {
        m_json_stream.close();
        m_ProfileCount = 0;
        closed = true;
        remove(m_json_filename.c_str());
    }

    void WriteProfile(const ProfileResult& result)
    {
        if(!closed)
        {
            /* next item timed */
            if (m_ProfileCount++ > 0)
            {
                m_json_stream << ",\n";
            }

            // creates timing entry to the JSON file that is read by Chrome.
            m_json_stream
                << "{\"cat\":\"function\",\"dur\":"<< (result.m_End - result.m_Start)
                << ",\"name\":\"" << result.m_Name
                << "\",\"ph\":\"X\",\"pid\":" << result.m_PID
                << ",\"tid\":" << result.m_ThreadID
                << ",\"ts\":" << result.m_Start << "}";

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

};

class ProfPool
{
    static constexpr size_t ROUNDS = 3;
public:

    // Could try to make this async
    void Log(ProfileResult&& result)
    {
        m_PushIndex++;

        // Run through queues to see if any are available for a push
        for(size_t queueIndex = 0; queueIndex != m_Queues.size() * ROUNDS; ++queueIndex)
        {
            if(m_Queues[(m_PushIndex + queueIndex) % m_Queues.size()].TryPush(std::move(result)))
            {
                return;
            }
        }

        // All queues are busy, just wait until free;
        m_Queues[m_PushIndex % m_Queues.size()].Push(std::move(result));
    }

    ProfPool(const std::string& json_profile_path = "PROCESS_PROFILE_RESULTS")
    : m_Profile_File_Name(json_profile_path + JSON_EXT),
      m_Profile_JSON(m_Profile_File_Name, std::ofstream::trunc), m_PushIndex(0),
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
        m_Profile_JSON << "{\"otherData\": {},\"traceEvents\":\n    [";
        m_Profile_JSON.flush();
    }


    void WriteProfileData()
    {
        bool first = true;

        if(!m_Profile_JSON.is_open())
        {
            std::cout << "m_Profile_JSON is not open!!\n";
        }

        for(ProfileWriter& writer: m_Writers)
        {
            if(!first)
            {
                m_Profile_JSON << ",";
            }
            first = false;
            m_Profile_JSON << writer;
            m_Profile_JSON.flush();
        }
    }

    void WriteFooter()
    {
        /* If the program dies before this can be called,
           "]}" can just be manually added to the end of the json file
        */

        if(!m_Profile_JSON.is_open())
        {
            std::cout << "m_Profile_JSON is not open!!\n";
        }

        m_Profile_JSON << "\n    ]\n}";
        m_Profile_JSON.close();
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
        std::string m_Profile_File_Name;
        std::ofstream m_Profile_JSON;
        size_t m_PushIndex;
        size_t m_NumQueues;
        std::vector<ProfileWriter> m_Writers;
        std::vector<ProfQueue> m_Queues;
};

class Timer
{

public:
    Timer(const char* name)
        :m_Name(name), m_Stopped(false), m_Start(now())
    {
        // Introduce timer delays so we can test optimizations
        // Delay everything except function you want to see if an
        // optimization would work.

    }

    ~Timer()
    {
        if (!m_Stopped)
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

        ProfileResult result = ProfileResult( m_Name, m_Start, endTime, threadID, pid );

        ProfPool::Instance().Log(std::move(result));
        m_Stopped = true;
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

    const std::string m_Name;
    bool m_Stopped;
    nanosec m_Start;
};

#endif