#ifndef DAEMON_THREAD_HH
#define DAEMON_THREAD_HH

#define _OLD_SCHOOL 0 // test using cpp version 0X


#include <pthread.h>


#include <future>
#include <chrono>


/*
 * Class that encapsulates promise and future object and
 * provides API to set exit signal for the thread
 */
template <typename... ARGS>
class DaemonThread
{
    bool m_Running;
#if _OLD_SCHOOL
    pthread_mutex_t m_Mutex;
    bool m_StopRequested;
#endif
    std::thread* m_RunningThread;
    std::promise<void> m_exitSignal;
    std::future<void> m_futureObj;

public:
    DaemonThread() :
        m_Running(false), m_RunningThread(nullptr), m_futureObj(m_exitSignal.get_future())
#if _OLD_SCHOOL
        , m_Mutex(PTHREAD_MUTEX_INITIALIZER), m_StopRequested(false)
#endif
        {
#if _OLD_SCHOOL
            pthread_mutex_init(&m_Mutex, NULL);
#endif
        }

    DaemonThread(DaemonThread && obj)
        : m_Running(std::move(obj.m_Running)), m_RunningThread(std::move(obj.m_RunningThread)), m_exitSignal(std::move(obj.m_exitSignal)), m_futureObj(std::move(obj.m_futureObj))
        { }

    DaemonThread & operator=(DaemonThread && obj)
    {
        m_exitSignal = std::move(obj.m_exitSignal);
        m_futureObj = std::move(obj.m_futureObj);
        m_Running = std::move(obj.m_Running);
        m_RunningThread = std::move(obj.m_RunningThread);
        return *this;
    }

    ~DaemonThread()
    {
#if _OLD_SCHOOL
        pthread_mutex_destroy(&m_Mutex);
#endif
    }

    // Task need to provide definition for this function
    // It will be called by thread function
    virtual void execute(ARGS... args) = 0;

    // Thread function to be executed by thread
    void operator()()
    {
        execute();
    }

    void start(ARGS... args)
    {
        m_RunningThread = new std::thread([this, args...]()
        {
                this->execute(args...);
        });

        m_Running = true;
    }

    //Checks if thread is requested to stop
    bool stopRequested()
    {

#if _OLD_SCHOOL
        bool stopped = false;

        if(pthread_mutex_trylock(&m_Mutex))
        {
            return false;
        }

        stopped = m_StopRequested;

        pthread_mutex_unlock(&m_Mutex);
        return stopped;
#else
        // checks if value in future object is available
        if (m_futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
        {
            return false;
        }

        return true;
#endif
    }

    // Request the thread to stop by setting value in promise object
    void stop()
    {
        if(m_Running)
        {
            m_Running = false;

#if _OLD_SCHOOL
            pthread_mutex_lock(&m_Mutex);
            m_StopRequested = true;
            pthread_mutex_unlock(&m_Mutex);
#else
            m_exitSignal.set_value();
#endif
            m_RunningThread->join();
            if(nullptr != m_RunningThread)
            {
                delete m_RunningThread;
                m_RunningThread = nullptr;
            }

        }
    }
};

#endif