#ifndef _PROFILER_HH
#define _PROFILER_HH

// View data by dragging JSON file to a chrome://tracing

/* Usage: Begin timing sessions by calling Instrumentor::Get().BeginSession($SESSION_NAME$, $JSON_OUTPUT_PATH$);*/
/* End session by calling  Instrumentor::Get().EndSession() */
/*
 Notes: If program dies before EndSession() is called, the function data will still be available,
 but will be missing a ]} at the end of the file. Append a ]} at the end and it will fix this issue.
 This cannot be controlled as destructors are not called upon a program being killed.
 */

/*
    Place a PROFILE_FUNCTION in any function once the Instrumentor is active
    to get info on when that function was called and how long it took.
    Note that putting this in every function may result in a json file too big to be opened.
    To time a specific section of a function you can limit the scope by enclosing
    the PROFILE_SCOPE($your_scope_name$) just to its own scope by adding {} around
    the scope you want tested.
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

#include <Logger.hh>
#include <DaemonThread.hh>

typedef uint64_t nanosec;

static const std::string JSON_EXT = ".json";

/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

// Because we're on C++0# and don't have std::to_string()..
// From: https://stackoverflow.com/questions/5590381/easiest-way-to-convert-int-to-string-in-c
#define SSTR( x ) static_cast< std::ostringstream & >( \
    ( std::ostringstream() << std::dec << x ) ).str()

// Profiling on 1; Profilling off 0
#define PROFILING 1
#if PROFILING


#define FUNCTION_SIG __PRETTY_FUNCTION__

// Macros to get function signatures in the JSON
#define PROFILE_SCOPE( NAME ) Timer timer##_LINE_( NAME )
#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNCTION_SIG)

#else

// If profiling is set to 0, disabled, don't compile
    #define PROFILE_SCOPE( NAME )
    #define PROFILE_FUNCTION()
#endif



struct ProfileResult
{
    ProfileResult(ProfileResult&& other)
        : Name(std::move(other.Name)), Start(std::move(other.Start)),
          End(std::move(other.End)), ThreadID(std::move(other.ThreadID)),
          PID(std::move(other.PID))
    {

    }

    ProfileResult& operator=(ProfileResult&& other)
    {
        Name = std::move(other.Name);
        Start = std::move(other.Start);
        End = std::move(other.End);
        ThreadID = std::move(other.ThreadID);
        PID = std::move(other.PID);
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

    public:
        ProfQueue() : m_Tasks{}, m_done(false), m_Mutex(PTHREAD_MUTEX_INITIALIZER)
        {
            pthread_mutex_init(&m_Mutex, NULL);
            pthread_cond_init(&m_Ready, NULL);
        }

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

            prof = std::move(m_Tasks.front());
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

            pthread_cond_signal(&m_Ready);
        }

        bool TryPush(ProfileResult&& prof)
        {
            if(pthread_mutex_trylock(&m_Mutex))
            {
                return false;
            }

            m_Tasks.push_back(std::move(prof));

            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_Ready);

            return true;
        }
};

class ProfileWriter : public DaemonThread<std::vector<ProfQueue>*, size_t>
{

private:
    int profileCount;
    bool closed;
    std::ofstream m_json_stream;

    ProfileWriter& operator=(const ProfileWriter&);

public:

    ProfileWriter(const ProfileWriter&) { }

    void execute(std::vector<ProfQueue>* results_queue, size_t writer_index)
    {
        ProfileResult result;
        result.End = 0;
        std::vector<ProfQueue>& queue = *results_queue;

        while (!stopRequested())
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
    }

    ProfileWriter(const std::string& filepath)
        : profileCount(0), closed(false), m_json_stream(filepath.c_str(), std::ios::out)

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
        profileCount = 0;
        closed = true;
    }

    void WriteProfile(const ProfileResult& result)
    {
        if(!closed)
        {
            /* next item timed */
            if (profileCount++ > 0)
            {
                m_json_stream << ",";
            }

            //std::string name = result.Name;
            //name.replace(name.begin(), name.end(), '"', '\'');

            // creates timing entry to the JSON file that is read by Chrome.
            m_json_stream
                << "{\"cat\":\"function\",\"dur\":"<< (result.End - result.Start)
                << ",\"name\":\"" << "TEST FUNCTION()"
                << "\",\"ph\":\"X\",\"pid\":" << result.PID
                << ",\"tid\":" << result.ThreadID
                << ",\"ts\":" << result.Start << "}";

            /* flush here so data isn't lost in case of crash */
            m_json_stream.flush();

            //std::cout << "Profiles so far: " << profileCount << std::endl;
        }
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
    // sysconf(_SC_NPROCESSORS_ONLN) only for POSIX Linux
    : profile_running(true), json_file_name(json_profile_path), m_PushIndex(0)
    {
        full_json.open((json_file_name + JSON_EXT).c_str());
        WriteHeader();

        for(size_t queue_index = 0; queue_index < 10; queue_index++)
        {
            m_Queues.emplace_back();
        }

        std::stringstream profile_name;
        for(size_t writer_index = 0; writer_index  < 10; writer_index++)
        {
            profile_name << "PROF_" << writer_index;
            m_Writers.emplace_back(profile_name.str());
            m_Writers.back().start(&m_Queues, writer_index);
            profile_name.str("");
        }
    }
    ~ProfPool()
    {
        WriteFooter();
        for(ProfQueue& queue : m_Queues)
        {
            queue.done();
        }

        for(ProfileWriter& writer: m_Writers)
        {
            writer.stop();
        }
    }

    void WriteHeader()
    {
        full_json << "{\"otherData\": {},\"traceEvents\":[";
        full_json.flush();
    }

    void WriteFooter()
    {
        #if 0
        /* If the program dies before this can be called,
            it can just be manually added to the end of the json file
        */
        /* @TODO have this added to the end of WriteProfile()
            so we don't have to worry about writing the footer
        */
        size_t thread_index = 0;
        profile_running = false;
        bool first = true;

        for(; thread_index < total_num_threads; ++thread_index)
        {
            m_ProfQueue[thread_index].end();
        }

        thread_index = 0;
        for(; thread_index < total_num_threads; ++thread_index)
        {
            pthread_join(m_Threads[thread_index], NULL);
        }

        thread_index = 0;
        for(; thread_index < total_num_threads; ++thread_index)
        {
            std::stringstream prof_path;
            prof_path << json_file_name.c_str() << "_" << SSTR(thread_index) << JSON_EXT;
            prof_json.open(prof_path.str());
            if(!first)
            {
                full_json << ",";
            }
            first = false;
            full_json << prof_json.rdbuf();
            prof_json.close();
        }

        full_json << "]}";
        full_json.flush();
        full_json.close();
        #endif
    }

    static ProfPool& Instance()
    {
        /* Singleton instance*/
        static ProfPool instance;
        return instance;
    }

private:
    ProfPool(const ProfPool&);
    ProfPool& operator=(const ProfPool&);

    private:
        std::ofstream full_json;
        std::ifstream prof_json;
        volatile bool profile_running;
        std::string json_file_name;
        std::vector<ProfileWriter> m_Writers;
        std::vector<ProfQueue> m_Queues;
        size_t m_PushIndex;
};

class Timer
{

public:
    Timer(const char* name)
        :Name(name), stopped(false), startTimepoint(now())
    {}

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

        ProfPool::Instance().Log(std::move(result));
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
            printf("Failed to obtain timestamp. errno = %i: %s\n", errno,
                strerror(errno));
            nanoseconds = UINT64_MAX; // use this to indicate error
        }
        else
        {
            nanoseconds = SEC_TO_NS((nanosec)ts.tv_sec) + (nanosec)ts.tv_nsec;
        }

        return nanoseconds;
    }
private:

    const std::string Name;
    bool stopped;
    nanosec startTimepoint;
};

#endif