#include "Dispatch.hpp"

#include <cassert>

#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"

// Definitions for dispatch event flags
#define DISPATCH_EXIT_EVT (0x2)

// This Bounce implementation is pulled from bounce.cpp
// This allows you to use a class method as a c function pointer
template <class T, class METHOD, METHOD M, class... PARAMS>
static auto bounce(void *priv, PARAMS... params)
    -> decltype(((*reinterpret_cast<T *>(priv)).*M)(params...)) {
    return ((*reinterpret_cast<T *>(priv)).*M)(params...);
}

// Convenience macro to simplify bounce statement usage
#define BOUNCE(c, m) bounce<c, decltype(&c::m), &c::m>

#pragma mark - Dispatch Class -

#pragma mark - Implementation -

// TODO: Make this configurable
#define QUEUE_SIZE 10

DispatchQueue::DispatchQueue(std::string name, size_t threadCnt, size_t threadStackSize,
                             UBaseType_t priority, BaseType_t core)
    : name_{std::move(name)}, threads_(threadCnt), notifyFlags_(xEventGroupCreate()) {

    assert(notifyFlags_ != nullptr && "Failed to create event group!");

    // Create the Queue
    // TODO: Not the best that we're passing pointers here, since then heap space is
    // alloc'd/dealloc'd all the time
    // NOLINTNEXTLINE(bugprone-sizeof-expression) - I promise I really want the size of the pointer!
    q_ = xQueueCreate(QUEUE_SIZE, sizeof(WorkItem *));

    // Dispatch thread setup
    for (size_t i = 0; i < threads_.size(); i++) {
        // Define the name
        threads_[i].name = StringFormat("D:%s%d", name_.c_str(), i);

        // Create the thread
        BaseType_t status = xTaskCreatePinnedToCore(
            reinterpret_cast<void (*)(void *)>(BOUNCE(DispatchQueue, dispatchThreadHandler)),
            threads_[i].name.c_str(), threadStackSize, reinterpret_cast<void *>(this), priority,
            &threads_[i].thread, core);
        assert(status == pdPASS && "Failed to create thread!");
    }
}

DispatchQueue::~DispatchQueue() {
    beginShutdown();
    waitForShutdown();
    // Cleanup event flags and mutex
    vEventGroupDelete(notifyFlags_);
}

void DispatchQueue::beginShutdown(bool graceful) {
    // Signal to dispatch threads that it's time to wrap up
    shutdownType_ = graceful ? ShutdownType::Graceful : ShutdownType::Immediate;

    for (int i = 0; i < threads_.size(); i++) {
        // Queue one quit signal for each thread
        auto quit = new WorkItem{.op = nullptr, .quit = true};
        xQueueSend(q_, (void *)&quit, portMAX_DELAY);
    }
}

auto DispatchQueue::waitForShutdown(int timeoutMs) -> bool {
    // Wait until a thread signals exit. Timeout is acceptable.
    auto bits = xEventGroupWaitBits(notifyFlags_, DISPATCH_EXIT_EVT, pdTRUE, pdFALSE,
                                    pdMS_TO_TICKS(timeoutMs));
    // Determine if we timed out by checking if DISPATCH_EXIT_EVT was not set
    if (bits & DISPATCH_EXIT_EVT) {
        return true;
    } else {
        log_w("Dispatch thread %s timed out on exit with %d items remaining!", name_.c_str(),
              size());
        return false;
        // TODO: kill the threads?
        // for (int i = 0; i < threads_.size(); i++) {
        //     vTaskDelete(threads_[i].thread);
        // }
    }
}

void DispatchQueue::dispatch(const fp_t &op) {
    if (shutdownType_ == ShutdownType::None) {
        auto work = new WorkItem{.op = op, .quit = false};
        xQueueSend(q_, (void *)&work, portMAX_DELAY);
    }
}

void DispatchQueue::dispatch(fp_t &&op) {
    if (shutdownType_ == ShutdownType::None) {
        auto work = new WorkItem{.op = std::move(op), .quit = false};
        xQueueSend(q_, (void *)&work, portMAX_DELAY);
    }
}

void DispatchQueue::dispatchFromISR(const fp_t &op) {
    if (shutdownType_ == ShutdownType::None) {
        auto work = new WorkItem{.op = op, .quit = false};
        xQueueSendFromISR(q_, (void *)&work, NULL);
    }
}

void DispatchQueue::dispatchFromISR(fp_t &&op) {
    if (shutdownType_ == ShutdownType::None) {
        auto work = new WorkItem{.op = std::move(op), .quit = false};
        xQueueSendFromISR(q_, (void *)&work, NULL);
    }
}

auto DispatchQueue::isInQueue() -> bool {
    for (auto &thread : threads_) {
        if (xTaskGetCurrentTaskHandle() == thread.thread) {
            return true;
        }
    }
    return false;
}

void DispatchQueue::dispatchThreadHandler() {
    // Wait for work
    WorkItem *work;
    do {
        xQueueReceive(q_, &(work), portMAX_DELAY);
        if (work->quit) {
            delete work;
            break;
        } else if (shutdownType_ != ShutdownType::Immediate) {
            // Execute the work
            work->op();
        }
        delete work;
    } while (shutdownType_ != ShutdownType::Immediate);

    // Set a signal to indicate a thread exited
    xEventGroupSetBits(notifyFlags_, DISPATCH_EXIT_EVT);

    log_d("Dispatch thread %s exiting...", name_.c_str());

    // Delete the current thread
    vTaskDelete(nullptr);
}

void DispatchQueue::dispatchOneOff(const fp_t &op, size_t threadStack, UBaseType_t priority,
                                   BaseType_t core) {
    auto work = new WorkItem{.op = op, .quit = false};
    xTaskCreatePinnedToCore(
        [](void *param) {
        auto work = reinterpret_cast<WorkItem *>(param);
        work->op();
        delete work;
        vTaskDelete(nullptr);
        },
        "D:OneOff", threadStack, work, priority, nullptr, core);
}

static DispatchQueue *core0BackgroundQueue = nullptr;
static DispatchQueue *core1BackgroundQueue = nullptr;
static DispatchQueue *uiQueue = nullptr;
static DispatchQueue *einkQueue = nullptr;

auto DispatchGetCore0BackgroundQueue(bool create) -> DispatchQueue * {
    if (core0BackgroundQueue == nullptr && create) {
        core0BackgroundQueue = new DispatchQueue("Core0BG", 1, 10000, 0, 0);
    }
    return core0BackgroundQueue;
}

auto DispatchGetCore1BackgroundQueue(bool create) -> DispatchQueue * {
    if (core1BackgroundQueue == nullptr && create) {
        core1BackgroundQueue = new DispatchQueue("Core1BG", 1, 10000, 0, 1);
    }
    return core1BackgroundQueue;
}

auto DispatchGetUiQueue(bool create) -> DispatchQueue * {
    if (uiQueue == nullptr && create) {
        uiQueue = new DispatchQueue("UI", 1, 10000, 10, 1);
    }
    return uiQueue;
}

auto DispatchGetEinkQueue(bool create) -> DispatchQueue * {
    if (einkQueue == nullptr && create) {
        einkQueue = new DispatchQueue("Eink", 1, 4096, 11, 1);
    }
    return einkQueue;
}

// Really this probably only needs to be the screens and the filesystem that we shut down
// gracefully. Everything else should be fine to just let die. We should consider moving all
// filesystem operations into their own queue and just shutting down that queue and the UI
// queue.
auto DispatchBeginShutdown() -> bool {
    bool isExecutingInQueue = false;
    auto core0BgQueue = DispatchGetCore0BackgroundQueue(false);
    if (core0BgQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || core0BgQueue->isInQueue();
        core0BgQueue->beginShutdown();
    }
    auto core1BgQueue = DispatchGetCore1BackgroundQueue(false);
    if (core1BgQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || core1BgQueue->isInQueue();
        core1BgQueue->beginShutdown();
    }
    auto uiQueue = DispatchGetUiQueue(false);
    if (uiQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || uiQueue->isInQueue();
        uiQueue->beginShutdown(true);
    }
    return isExecutingInQueue;
}

auto DispatchWaitForShutdown(int timeoutMs) -> bool {
    auto startMs = Millis();
    bool success = true;
    log_d("Waiting for dispatch queues to shutdown");
    auto core0BgQueue = DispatchGetCore0BackgroundQueue(false);
    if (core0BgQueue && !core0BgQueue->isInQueue()) {
        success &= core0BgQueue->waitForShutdown(timeoutMs);
    }
    auto core1BgQueue = DispatchGetCore1BackgroundQueue(false);
    if (core1BgQueue && !core1BgQueue->isInQueue()) {
        int remainingMs = timeoutMs - std::max(0, Millis() - startMs);
        success &= core1BgQueue->waitForShutdown(remainingMs);
    }
    auto uiQueue = DispatchGetUiQueue(false);
    if (uiQueue && !uiQueue->isInQueue()) {
        int remainingMs = timeoutMs - std::max(0, Millis() - startMs);
        success &= uiQueue->waitForShutdown(remainingMs);
    }
    // Eink queue is a special case. We must wait for it to gracefully shutdown _after_ the UI queue
    // has already shutdown Once other queues are shutdown we can shutdown the eink queue
    auto einkQueue = DispatchGetEinkQueue(false);
    if (einkQueue) {
        einkQueue->beginShutdown(true);
        int remainingMs = timeoutMs - std::max(0, Millis() - startMs);
        success &= einkQueue->waitForShutdown(remainingMs);
    }

    log_d("Dispatch queues shutdown");
    return success;
}

auto DispatchShutdown(int timeoutMs) -> bool {
    DispatchBeginShutdown();
    return DispatchWaitForShutdown(timeoutMs);
}
