#ifndef DAEMON_THREAD_HH
#define DAEMON_THREAD_HH

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
    std::thread m_RunningThread;
    std::promise<void> m_exitSignal;
    std::future<void> m_futureObj;
public:
    DaemonThread() :
        m_Running(false), m_RunningThread{}, m_futureObj(m_exitSignal.get_future())
        { }

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
        m_Running = true;
        m_RunningThread = std::thread([this, args...]()
        {
                execute(args...);
        });

    }

    //Checks if thread is requested to stop
    bool stopRequested()
    {
        // checks if value in future object is available
        if (m_futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
        {
            return false;
        }

        return true;
    }

    // Request the thread to stop by setting value in promise object
    void stop()
    {
        if(m_Running)
        {
            m_Running = false;
            m_exitSignal.set_value();
            m_RunningThread.join();

        }
    }
};

#endif