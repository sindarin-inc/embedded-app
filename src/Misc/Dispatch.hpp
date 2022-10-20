#include <functional>
// #include <thread>
#include <vector>
// #include <cstdint>
// #include <cstdio>
#include <queue>
// #include <mutex>
// #include <string>
// #include <condition_variable>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
// #include <cassert>

// Based on
// https://embeddedartistry.com/blog/2018/01/29/implementing-an-asynchronous-dispatch-queue-with-freertos/

struct freertos_thread_t {
    TaskHandle_t thread;
    std::string name;
};

class dispatch_queue {
    typedef std::function<void(void)> fp_t;

public:
    dispatch_queue(std::string name, size_t thread_cnt = 1, size_t thread_stack = 1024,
                   UBaseType_t priority = 5, BaseType_t core = 1);
    ~dispatch_queue();

    // dispatch and copy
    void dispatch(const fp_t &op);
    // dispatch and move
    void dispatch(fp_t &&op);

    // Deleted operations
    dispatch_queue(const dispatch_queue &rhs) = delete;
    dispatch_queue &operator=(const dispatch_queue &rhs) = delete;
    dispatch_queue(dispatch_queue &&rhs) = delete;
    dispatch_queue &operator=(dispatch_queue &&rhs) = delete;

private:
    std::string name_;

    // FreeRTOS uses semaphore handles for mutexes
    SemaphoreHandle_t mutex_;

    /// Vector of FreeRTOS Threads
    std::vector<freertos_thread_t> threads_;

    /// FreeRTOS event flags - like condition variable, used for waking queue threads
    EventGroupHandle_t notify_flags_;

    std::queue<fp_t> q_;
    bool quit_ = false;

    void dispatch_thread_handler(void);
};

dispatch_queue *dispatch_get_core0_background_queue();

dispatch_queue *dispatch_get_ui_queue();
