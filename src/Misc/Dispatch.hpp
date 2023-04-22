#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <functional>
#include <queue>
#include <string>
#include <vector>

// Based on
// https://embeddedartistry.com/blog/2018/01/29/implementing-an-asynchronous-dispatch-queue-with-freertos/

struct FreertosThreadT {
    TaskHandle_t thread;
    std::string name;
};

class DispatchQueue {
    typedef std::function<void(void)> fp_t;
    struct WorkItem {
        fp_t op;
        bool quit;
    };

public:
    DispatchQueue(std::string name, size_t threadCnt = 1, size_t threadStack = 1024,
                  UBaseType_t priority = 5, BaseType_t core = 1);
    ~DispatchQueue();

    void beginShutdown();
    void waitForShutdown(int timeoutMs = 10);

    // dispatch and copy
    void dispatch(const fp_t &op);
    void dispatchFromISR(const fp_t &op);
    // dispatch and move
    void dispatch(fp_t &&op);
    void dispatchFromISR(fp_t &&op);

    // Returns true if were' currently executing inside this queue
    auto isInQueue() -> bool;

    // Number of items on the queue
    auto size() -> int { return uxQueueMessagesWaiting(q_); }
    auto waitNotEmpty(int timeoutMs = 10) -> bool {
        WorkItem dummy;
        return xQueuePeek(q_, &dummy, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
    }

    // Deleted operations
    DispatchQueue(const DispatchQueue &rhs) = delete;
    auto operator=(const DispatchQueue &rhs) -> DispatchQueue & = delete;
    DispatchQueue(DispatchQueue &&rhs) = delete;
    auto operator=(DispatchQueue &&rhs) -> DispatchQueue & = delete;

    // Fires up a task for only the purpose of running a single function.
    // Handy since you can use lambda captures with it.
    static void dispatchOneOff(const fp_t &op, size_t threadStack = 1024, UBaseType_t priority = 5,
                               BaseType_t core = 1);

private:
    std::string name_;

    // Vector of FreeRTOS Threads
    std::vector<FreertosThreadT> threads_;

    // FreeRTOS event flags - used for waiting for thread deletion
    EventGroupHandle_t notifyFlags_;

    QueueHandle_t q_;
    bool quit_ = false;

    void dispatchThreadHandler();
};

auto DispatchGetCore0BackgroundQueue(bool create = true) -> DispatchQueue *;

auto DispatchGetCore1BackgroundQueue(bool create = true) -> DispatchQueue *;

auto DispatchGetUiQueue(bool create = true) -> DispatchQueue *;
