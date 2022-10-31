#include <mutex>
#include <condition_variable>
#include <queue>

template <typename Data>
class ThreadSafeQueue {
    std::mutex m_Mutex;
    std::condition_variable m_Conditionvariable;
    std::queue<Data> m_Queue;

public:
    void push(Data&& item) {
        {
            std::lock_guard lock(m_Mutex);
            m_Queue.push(item);
        }

        m_Conditionvariable.notify_one();
    }

    Data& front() {
        std::unique_lock lock(m_Mutex);
        m_Conditionvariable.wait(lock, [&]{ return !m_Queue.empty(); });
        return m_Queue.front();
    }

    void pop() {
        std::lock_guard lock(m_Mutex);
        m_Queue.pop(item);
    }
};