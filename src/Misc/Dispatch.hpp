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
    TaskHandle_t thread{};
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

    // Begin the queue shutdown. If graceful==false, this will finish executing the current task but
    // all future tasks will be ignored. If graceful==true, this will wait for all tasks to finish
    // executing before shutting down.
    void beginShutdown(bool graceful = false);
    auto waitForShutdown(int timeoutMs = 10) -> bool;

    // dispatch and copy
    void dispatch(const fp_t &op);
    void dispatchFromISR(const fp_t &op);
    // dispatch and move
    void dispatch(fp_t &&op);
    void dispatchFromISR(fp_t &&op);

    // dispatch and wait for completion
    void dispatchSync(fp_t &&op, TickType_t timeout = portMAX_DELAY);

    // Returns true if were' currently executing inside this queue
    auto isInQueue() -> bool;

    // Number of items on the queue
    auto size() -> int { return uxQueueMessagesWaiting(q_); }
    auto waitNotEmpty(int timeoutMs = 10) -> bool {
        WorkItem *queued;
        auto hasItem = xQueuePeek(q_, &queued, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
        // Only report the queue as not empty if it's not a quit signal that's queued
        return hasItem && !queued->quit;
    }
    auto waitEmpty(TickType_t timeout) -> bool {
        TickType_t startTime = xTaskGetTickCount();
        while (uxQueueMessagesWaiting(q_) > 0) {
            if (xTaskGetTickCount() - startTime >= timeout) {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return true;
    }

    // Deleted operations
    DispatchQueue(const DispatchQueue &rhs) = delete;
    auto operator=(const DispatchQueue &rhs) -> DispatchQueue & = delete;
    DispatchQueue(DispatchQueue &&rhs) = delete;
    auto operator=(DispatchQueue &&rhs) -> DispatchQueue & = delete;

    // Fires up a task for only the purpose of running a single function.
    // Handy since you can use lambda captures with it.
    static auto dispatchOneOff(const fp_t &op, size_t threadStack = 1024, UBaseType_t priority = 5,
                               BaseType_t core = 1, const char *name = "OneOff") -> bool;
    // Like dispatchOneOff, but waits for the function to complete before returning. Useful for
    // synchronously running things on other cores.
    static auto dispatchOneOffSync(const fp_t &op, size_t threadStack, UBaseType_t priority,
                                   BaseType_t core, const char *name,
                                   TickType_t timeout = portMAX_DELAY) -> bool;

private:
    std::string name_;

    // Vector of FreeRTOS Threads
    std::vector<FreertosThreadT> threads_;

    // FreeRTOS event flags - used for waiting for thread deletion
    EventGroupHandle_t notifyFlags_;

    QueueHandle_t q_;

    enum class ShutdownType {
        None = 0,
        Graceful = 1,
        Immediate = 2,
    };

    ShutdownType shutdownType_ = ShutdownType::None;

    void dispatchThreadHandler();
};

auto DispatchGetCore0BackgroundQueue(bool create = true) -> DispatchQueue *;

auto DispatchGetCore1BackgroundQueue(bool create = true) -> DispatchQueue *;

auto DispatchGetUiQueue(bool create = true) -> DispatchQueue *;

auto DispatchGetEinkQueue(bool create = true) -> DispatchQueue *;

// Returns true if we're currently inside one of the queues
auto DispatchBeginShutdown() -> bool;

auto DispatchWaitForShutdown(int timeoutMs) -> bool;

auto DispatchShutdown(int timeoutMs = 10000) -> bool;
