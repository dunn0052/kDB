#ifndef _PTHREAD_PROFILER_HH
#define _PTHREAD_PROFILER_HH

/* View data by loading JSON file to chrome://tracing

 * For CPP 0X -- The profiler must be initialized in the main thread or
 * there is danger of the ProfPool being multi-initialized.

 * Usage: Call PROFILE_FUNCTION() or PROFILE_SCOPE() to start profiling.
 * START_PROFILING() can be called to start the profiler threads without
 * logging profile metrics.

 * Notes: If program dies before the profiler destructor is called the
 * individual writer files will still be around and you can manually combine
 * all of them together.
 */

#include <string>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <unistd.h> // getpid()
#include <sys/syscall.h> // __NR_getid
#include <time.h>
#include <queue>

// Program name for profile json
// Not portable outside of glibc
extern char *program_invocation_name;
extern char *program_invocation_short_name;

typedef uint64_t nanosec;

static const std::string JSON_EXT = ".json";




// Profiling on 1; Profiling off 0
#define PROFILING 1
#ifdef PROFILING


// linux only!!
#define FUNCTION_SIG __PRETTY_FUNCTION__

// Profile a named scope like thread loop
#define PROFILE_SCOPE( NAME ) Timer timer##_LINE_( NAME )
// Profile with function name - place at function entry
#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNCTION_SIG)
// Used for intializing profiling without profiling the scope itself
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
        : m_Name(other.m_Name), m_Start(other.m_Start),
          m_End(other.m_End), m_ThreadID(other.m_ThreadID),
          m_PID(other.m_PID)
    {

    }

    ProfileResult& operator=(ProfileResult& other)
    {
        m_Name = other.m_Name;
        m_Start = other.m_Start;
        m_End = other.m_End;
        m_ThreadID = other.m_ThreadID;
        m_PID = other.m_PID;
        return *this;
    }


    ProfileResult(const std::string& name, nanosec& start, nanosec& end,
                  pid_t& threadid, pid_t& pid)
        : m_Name(name), m_Start(start), m_End(end), m_ThreadID(threadid),
          m_PID(pid)
        {

        }

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
        std::queue<ProfileResult> m_ResultQueue;
        bool m_Done;
        pthread_mutex_t m_Mutex;
        pthread_cond_t m_ReadyCondition;

        ProfQueue& operator=(const ProfQueue&);

    public:
        ProfQueue()
            : m_ResultQueue(), m_Done(false), m_Mutex()
        {
            pthread_mutex_init(&m_Mutex, NULL);
            pthread_cond_init(&m_ReadyCondition, NULL);
        }

        ProfQueue(const ProfQueue& other)
            : m_ResultQueue(other.m_ResultQueue), m_Done(other.m_Done),
              m_Mutex(other.m_Mutex)
        {

        }

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

        bool Pop(ProfileResult& result)
        {
            pthread_mutex_lock(&m_Mutex);
            while(m_ResultQueue.empty() && !m_Done)
            {
                pthread_cond_wait(&m_ReadyCondition, &m_Mutex);
            }

            if(m_ResultQueue.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                result.m_End = 0; // Denote empty result
                return false;
            }

            result = m_ResultQueue.front();
            m_ResultQueue.pop();

            pthread_mutex_unlock(&m_Mutex);
            return true;
        }

        bool TryPop(ProfileResult& result)
        {

            if(pthread_mutex_trylock(&m_Mutex))
            {
                result.m_End = 0;
                return false;
            }

            if(m_ResultQueue.empty())
            {
                pthread_mutex_unlock(&m_Mutex);
                result.m_End = 0;
                return false;
            }

            result = m_ResultQueue.front();
            m_ResultQueue.pop();

            pthread_mutex_unlock(&m_Mutex);

            return true;
        }

        void Push(ProfileResult& result)
        {
            pthread_mutex_lock(&m_Mutex);
            m_ResultQueue.push(result);
            pthread_mutex_unlock(&m_Mutex);

            pthread_cond_signal(&m_ReadyCondition);
        }

        bool TryPush(ProfileResult& result)
        {
            if(pthread_mutex_trylock(&m_Mutex))
            {
                return false;
            }

            m_ResultQueue.push(result);

            pthread_mutex_unlock(&m_Mutex);
            pthread_cond_signal(&m_ReadyCondition);

            return true;
        }
};

class ProfileWriter
{

public:

    int Start(std::vector<ProfQueue>* results_queue)
    {
        int thread_retcode = 0;

        if(!m_Running)
        {
            m_ResultsQueue = results_queue;

            thread_retcode = pthread_create(&m_ThreadID, NULL,
                ProfileWriter::thread_starter, static_cast<void*>(this));

            m_Running = 0 == thread_retcode; // non-zero on error
        }

        return thread_retcode;
    }

    void Stop()
    {
        if(m_Running)
        {
            m_Running = false;

            // Wait for lock here to avoid clashing with StopRequested()
            pthread_mutex_lock(&m_Mutex);
            m_StopRequested = true;
            pthread_mutex_unlock(&m_Mutex);

            // A stop has been requested so will finish up queue and exit
            pthread_join(m_ThreadID, NULL);

            pthread_mutex_destroy(&m_Mutex);

            m_ThreadID = 0;
        }
    }

    void execute()
    {

        ProfileResult result;
        result.m_End = 0; // Denote empty result

        std::vector<ProfQueue>& queue = *m_ResultsQueue;

        if(!m_JSON_Stream.is_open())
        {
            m_JSON_Stream.open(m_json_filename.c_str(), std::ofstream::trunc);
        }

        while (!StopRequested())
        {
            // Try and steal requests from other busy queues
            for(size_t queueIndex = 0; queueIndex != queue.size(); ++ queueIndex)
            {
                if(queue[(queueIndex + m_Writer_Index) % queue.size()].TryPop(result))
                {
                    break;
                }
            }

            // If TryPop() didn't produce anything then wait to Pop()
            if( (0 == result.m_End) && (!queue[m_Writer_Index].Pop(result)) )
            {
                // If Pop() returns false then a stop has been requested
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

    friend std::ofstream& operator<< (std::ofstream& os, ProfileWriter& p)
    {
        std::string line;
        std::ifstream profile_data;

        if(!p.m_JSON_Stream.is_open())
        {
            p.m_JSON_Stream.close();
        }

        profile_data.open(p.m_json_filename.c_str());
        if(!profile_data.is_open())
        {
            os << "Could not open: " << p.m_json_filename << "!\n";
            return os;
        }

        while(getline(profile_data, line)){
            os << "\n        " << line;
        }

        profile_data.close();

        return os;
    }

    ProfileWriter& operator=(const ProfileWriter& other)
    {
       m_FirstEntry = other.m_FirstEntry;
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
     : m_FirstEntry(other.m_FirstEntry),
       m_json_filename(other.m_json_filename),
       m_Writer_Index(other.m_Writer_Index), m_Mutex(other.m_Mutex),
       m_StopRequested(other.m_StopRequested), m_ThreadID(other.m_ThreadID),
       m_Running(other.m_Running), m_ResultsQueue(other.m_ResultsQueue)
    {

    }

    ProfileWriter(const std::string& filepath, size_t writer_id)
        : m_FirstEntry(true), m_json_filename(filepath),
          m_Writer_Index(writer_id), m_Mutex(), m_StopRequested(false),
          m_ThreadID(0), m_Running(false), m_ResultsQueue(NULL)
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

private:

    void EndSession()
    {
        remove(m_json_filename.c_str());
    }

    void WriteProfile(const ProfileResult& result)
    {
        /* next item timed */
        if (!m_FirstEntry)
        {
            m_JSON_Stream << ",\n";
        }
        else
        {
            m_FirstEntry = false;
        }

        // Format for chrome://tracing entry
        m_JSON_Stream
            << "{\"cat\":\"function\",\"dur\":"<< (result.m_End - result.m_Start)
            << ",\"name\":\"" << result.m_Name
            << "\",\"ph\":\"X\",\"pid\":" << result.m_PID
            << ",\"tid\":" << result.m_ThreadID
            << ",\"ts\":" << result.m_Start
            << "}";

        /* flush here so data isn't lost in case of crash */
        m_JSON_Stream.flush();
    }

    bool StopRequested()
    {
        bool stopped = false;

        /* If we're using the stop mutex it's safe to just
         *  ignore the check until the next cycle
         */
        if(pthread_mutex_trylock(&m_Mutex))
        {
            return false;
        }

        stopped = m_StopRequested;

        pthread_mutex_unlock(&m_Mutex);

        return stopped;
    }

    static void* thread_starter(void* self)
    {
        static_cast<ProfileWriter*>(self)->execute();
        return NULL;
    }

    bool m_FirstEntry;
    std::ofstream m_JSON_Stream;
    std::string m_json_filename;
    size_t m_Writer_Index;
    pthread_mutex_t m_Mutex;
    bool m_StopRequested;
    pthread_t m_ThreadID;
    bool m_Running;
    std::vector<ProfQueue>* m_ResultsQueue;

};

class ProfPool
{
    // Number of times to spin around push queues to find a non-busy queue
    // This value is only a guess and may be experimented on to tweak for
    // optimal performance
    static const size_t ROUNDS = 3;

public:

    void Log(ProfileResult& result)
    {
        if(!m_Ready)
        {
            // Bail out to avoid accessing the queue array
            return;
        }

        // Keep moving where we start pushing to spread between queues
        m_PushIndex++; // May need to reset to avoid possible integer overflow?

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

    // Destructor called on program exit
    // Writes final data so may lag closing the profiled program
    ~ProfPool()
    {
        if(m_Ready)
        {
            Stop();
            WriteProfileData();
            WriteFooter();
            m_Profile_JSON.close();
        }
    }

#define SSTR( x ) static_cast< std::ostringstream & >( \
    ( std::ostringstream() << std::dec << x ) ).str()

    static ProfPool& Instance()
    {
        /* Singleton instance - must be initialized in main thread */
        static ProfPool instance(
            std::string(program_invocation_short_name) + "_" + SSTR(getpid()));
        return instance;
    }

#undef SSTR

private:

    ProfPool(const std::string& json_profile_path = "PROCESS_PROFILE_RESULTS")
    : m_Profile_File_Name(json_profile_path + JSON_EXT),
      m_PushIndex(0), m_NumQueues(4/*sysconf(_SC_NPROCESSORS_ONLN) - 1*/),
      m_Queues(m_NumQueues), m_Ready(false)
    {
        m_Profile_JSON.open(m_Profile_File_Name.c_str(), std::ofstream::trunc);
        if(!m_Profile_JSON.is_open())
        {
            std::cout << "Could not create: "
                      << m_Profile_File_Name
                      << "\nProfiling FAILED!";
            m_Ready = false;
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
            ProfileWriter writer = ProfileWriter(profile_name.str(), writer_index);
            m_Writers.push_back(writer);
            profile_name.str("");
        }

        int thread_retcode = 0;
        // Must start ONLY after all are created to avoid deadlocks
        for(size_t writer_index = 0; writer_index < m_NumQueues; writer_index++)
        {
            thread_retcode = m_Writers[writer_index].Start(&m_Queues);
            if(0 != thread_retcode)
            {
                // Something went wrong and deadlocks will occur. Just bail out.
                m_Profile_JSON << "Profiler startup FAILED!";
                m_Profile_JSON.close();
                Stop();
                m_Ready = false;
                return;
            }
        }

        m_Ready = true;
    }

    void Stop()
    {
        // Must be done before we stop writers
        for(size_t queue_index = 0; queue_index < m_Queues.size(); queue_index++)
        {
            m_Queues[queue_index].done();
        }

        for(size_t writer_index = 0; writer_index < m_Writers.size(); writer_index++)
        {
            m_Writers[writer_index].Stop();
        }
    }

    void WriteHeader()
    {
        m_Profile_JSON << "{\"otherData\": {},\"traceEvents\":\n    [";
        m_Profile_JSON.flush();
    }

    void WriteProfileData()
    {
        bool first_line = true;

        for(size_t writer_index = 0; writer_index < m_Writers.size(); writer_index++)
        {
            ProfileWriter& writer = m_Writers[writer_index];
            if(!first_line)
            {
                m_Profile_JSON << ",";
            }

            first_line = false;
            m_Profile_JSON << writer;

            // Flush data in case of crash we have it
            m_Profile_JSON.flush();
        }
    }

    void WriteFooter()
    {
        /* If the program dies before this can be called,
           "]}" can just be manually added to the end of the json file
        */

        m_Profile_JSON << "\n    ]\n}";
    }

    // Private to enforce singelton pattern
    ProfPool(const ProfPool&);
    ProfPool& operator=(const ProfPool&);

    private:
        std::string m_Profile_File_Name;
        std::ofstream m_Profile_JSON;
        size_t m_PushIndex;
        size_t m_NumQueues;
        std::vector<ProfileWriter> m_Writers;
        std::vector<ProfQueue> m_Queues;
        bool m_Ready;
};

// Probe for timing - Starts at construction and stops on destruction
class Timer
{

public:
    Timer(const char* name)
        :m_Name(name), m_Start(now())
    {
        // Introduce timer delays so we can test optimizations
        // Delay everything except function you want to see if an
        // optimization would work.
    }

    ~Timer()
    {
        // Stop in destructor to measure full function time
        Stop();
    }


private:

    void Stop()
    {
        nanosec end_time = now();

        // This is Linux specific!!
        pid_t threadID = syscall(__NR_gettid);

        pid_t pid = getpid();

        ProfileResult result = ProfileResult( m_Name, m_Start, end_time, threadID, pid );

        ProfPool::Instance().Log(result);
    }

#define SEC_TO_NS(SEC) (static_cast<nanosec>(SEC) * 1000000000U)

    nanosec now()
    {
        nanosec nano_seconds = 0; // If unchanged then inidcates error
        struct timespec ts;
        int return_code = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

        if (-1 != return_code)
        {
            nano_seconds = SEC_TO_NS(ts.tv_sec) +
                static_cast<nanosec>(ts.tv_nsec);
        }

        return nano_seconds;
    }
#undef SEC_TO_NS

    const std::string m_Name;
    nanosec m_Start;
};

#endif