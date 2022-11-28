#include "Dispatch.hpp"

#include <cassert>

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
    : name_{std::move(name)}, threads_(threadCnt) {

    // Create the event flags
    notifyFlags_ = xEventGroupCreate();
    assert(notifyFlags_ != nullptr && "Failed to create event group!");

    // Create the Queue
    // TODO: Not the best that we're passing pointers here, since then heap space is
    // alloc'd/dealloc'd all the time
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
    // Signal to dispatch threads that it's time to wrap up
    quit_ = true;

    // We will join each thread to confirm exiting
    for (int i = 0; i < threads_.size(); i++) {
        // Queue one quit signal for each thread
        auto quit = new WorkItem{.op = nullptr, .quit = true};
        xQueueSend(q_, (void *)&quit, portMAX_DELAY);

        // Wait until a thread signals exit. Timeout is acceptable.
        xEventGroupWaitBits(notifyFlags_, DISPATCH_EXIT_EVT, pdTRUE, pdFALSE, 10);
    }

    // Cleanup event flags and mutex
    vEventGroupDelete(notifyFlags_);
}

void DispatchQueue::dispatch(const fp_t &op) {
    auto work = new WorkItem{.op = op, .quit = false};
    xQueueSend(q_, (void *)&work, portMAX_DELAY);
}

void DispatchQueue::dispatch(fp_t &&op) {
    auto work = new WorkItem{.op = std::move(op), .quit = false};
    xQueueSend(q_, (void *)&work, portMAX_DELAY);
}

void DispatchQueue::dispatchFromISR(const fp_t &op) {
    auto work = new WorkItem{.op = op, .quit = false};
    xQueueSendFromISR(q_, (void *)&work, NULL);
}

void DispatchQueue::dispatchFromISR(fp_t &&op) {
    auto work = new WorkItem{.op = std::move(op), .quit = false};
    xQueueSendFromISR(q_, (void *)&work, NULL);
}

void DispatchQueue::dispatchThreadHandler() {
    // Wait for work
    WorkItem *work;
    do {
        xQueueReceive(q_, &(work), portMAX_DELAY);
        if (!work->quit && !quit_) {
            // Execute the work
            work->op();
        }
        delete work;
    } while (!quit_);

    // Set a signal to indicate a thread exited
    auto status = xEventGroupSetBits(notifyFlags_, DISPATCH_EXIT_EVT);
    assert(status == pdTRUE && "Failed to set event flags!");

    // Delete the current thread
    vTaskDelete(nullptr);
}

static DispatchQueue *core0BackgroundQueue = nullptr;
static DispatchQueue *core1BackgroundQueue = nullptr;
static DispatchQueue *uiQueue = nullptr;

auto DispatchGetCore0BackgroundQueue() -> DispatchQueue * {
    if (core0BackgroundQueue == nullptr) {
        core0BackgroundQueue = new DispatchQueue("Core0BG", 1, 10000, 0, 0);
    }
    return core0BackgroundQueue;
}

auto DispatchGetCore1BackgroundQueue() -> DispatchQueue * {
    if (core1BackgroundQueue == nullptr) {
        core1BackgroundQueue = new DispatchQueue("Core1BG", 1, 10000, 0, 1);
    }
    return core1BackgroundQueue;
}

auto DispatchGetUiQueue() -> DispatchQueue * {
    if (uiQueue == nullptr) {
        uiQueue = new DispatchQueue("UI", 1, 10000, 10, 1);
    }
    return uiQueue;
}
