#include <retcode.hh>
#include <Logger.hh>
#include <CLI.hh>
#include <profiler.hh>

#include <cstdlib>

void thread_func(int max_sleep_time)
{
    for(int i = 0; i < 1000; i++)
    {
        PROFILE_FUNCTION();

        //int sleep_time =  std::rand() % (max_sleep_time * 10);

        // doing "work"
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        //std::cout << "end thread: " << syscall(__NR_gettid) << std::endl;
    }
}


int main(int argc, char* argv[])
{
    PROFILE_FUNCTION();
    static const bool running = true;
    static uint64_t loop_count = 0;

    std::vector<std::thread> threads;

    for(int i = 0; i < 10; i++)
    {
        threads.push_back(std::thread(thread_func, 10));
    }

    for(std::thread& thread : threads)
    {
        thread.join();
    }

    std::cout << "End profiling!\n";
}