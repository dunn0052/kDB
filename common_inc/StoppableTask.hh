#ifndef DAEMON_THREAD_HH
#define DAEMON_THREAD_HH

#include <future>
#include <chrono>

/*
 * Class that encapsulates promise and future object and
 * provides API to set exit signal for the thread
 */
template <typename ARGS>
class DaemonThread
{
    std::promise<void> exitSignal;
    std::future<void> futureObj;
public:
    DaemonThread()
        : futureObj(exitSignal.get_future())
        { }

    DaemonThread(DaemonThread && obj)
        : exitSignal(std::move(obj.exitSignal)), futureObj(std::move(obj.futureObj))
        { }

    DaemonThread & operator=(DaemonThread && obj)
    {
        exitSignal = std::move(obj.exitSignal);
        futureObj = std::move(obj.futureObj);
        return *this;
    }

    // Task need to provide definition for this function
    // It will be called by thread function
    virtual void run(ARGS args) = 0;

    // Thread function to be executed by thread
    void operator()()
    {
        run();
    }

    //Checks if thread is requested to stop
    bool stopRequested()
    {
        // checks if value in future object is available
        if (futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
        {
            return false;
        }

        return true;
    }

    // Request the thread to stop by setting value in promise object
    void stop()
    {
        exitSignal.set_value();
    }
};

#endif