#include "Dispatch.hpp"

#include <cassert>

// Definitions for dispatch event flags
#define DISPATCH_WAKE_EVT (0x1)
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

DispatchQueue::DispatchQueue(std::string name, size_t threadCnt, size_t threadStackSize,
                             UBaseType_t priority, BaseType_t core)
    : name_{std::move(name)}, threads_(threadCnt) {
    // Create the Mutex
    mutex_ = xSemaphoreCreateRecursiveMutex();
    assert(mutex_ != nullptr && "Failed to create mutex!");

    // Create the event flags
    notifyFlags_ = xEventGroupCreate();
    assert(notifyFlags_ != nullptr && "Failed to create event group!");

    // Dispatch thread setup
    for (size_t i = 0; i < threads_.size(); i++) {
        // Define the name
        threads_[i].name = std::string("Dispatch Thread " + std::to_string(i));

        // Create the thread
        // BaseType_t status = xTaskCreate(
        //     reinterpret_cast<void (*)(void *)>(BOUNCE(dispatch_queue, dispatch_thread_handler)),
        //     threads_[i].name.c_str(), thread_stack_size, reinterpret_cast<void *>(this),
        //     priority, &threads_[i].thread);
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
    for (auto &thread : threads_) {
        eTaskState state;

        do {
            // Signal wake - check exit flag
            xEventGroupSetBits(notifyFlags_, DISPATCH_WAKE_EVT);

            // Wait until a thread signals exit. Timeout is acceptable.
            xEventGroupWaitBits(notifyFlags_, DISPATCH_EXIT_EVT, pdTRUE, pdFALSE, 10);

            // If it was not thread_[i], that is ok, but we will loop around
            // until threads_[i] has exited
            state = eTaskGetState(thread.thread);
        } while (state != eDeleted);

        thread.name.clear();
    }

    // Cleanup event flags and mutex
    vEventGroupDelete(notifyFlags_);

    vSemaphoreDelete(mutex_);
}

void DispatchQueue::dispatch(const fp_t &op) {
    BaseType_t status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    assert(status == pdTRUE && "Failed to lock mutex!");

    q_.push(op);

    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Notifies threads that new work has been added to the queue
    xEventGroupSetBits(notifyFlags_, DISPATCH_WAKE_EVT);
}

void DispatchQueue::dispatch(fp_t &&op) {
    BaseType_t status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    assert(status == pdTRUE && "Failed to lock mutex!");

    q_.push(std::move(op));

    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Notifies threads that new work has been added to the queue
    xEventGroupSetBits(notifyFlags_, DISPATCH_WAKE_EVT);
}

void DispatchQueue::dispatchThreadHandler() {
    BaseType_t status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    assert(status == pdTRUE && "Failed to lock mutex!");

    do {
        // after wait, we own the lock
        if (!quit_ && q_.size()) {
            auto op = std::move(q_.front());
            q_.pop();

            // unlock now that we're done messing with the queue
            status = xSemaphoreGiveRecursive(mutex_);
            assert(status == pdTRUE && "Failed to unlock mutex!");

            op();

            status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
            assert(status == pdTRUE && "Failed to lock mutex!");
        } else if (!quit_) {
            status = xSemaphoreGiveRecursive(mutex_);
            assert(status == pdTRUE && "Failed to unlock mutex!");

            // Wait for new work - clear flags on exit
            xEventGroupWaitBits(notifyFlags_, DISPATCH_WAKE_EVT, pdTRUE, pdFALSE, portMAX_DELAY);

            status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
            assert(status == pdTRUE && "Failed to lock mutex!");
        }
    } while (!quit_);

    // We were holding the mutex after we woke up
    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Set a signal to indicate a thread exited
    status = xEventGroupSetBits(notifyFlags_, DISPATCH_EXIT_EVT);
    assert(status == pdTRUE && "Failed to set event flags!");

    // Delete the current thread
    vTaskDelete(nullptr);
}

static DispatchQueue *core0BackgroundQueue = nullptr;
static DispatchQueue *uiQueue = nullptr;

auto DispatchGetCore0BackgroundQueue() -> DispatchQueue * {
    if (core0BackgroundQueue == nullptr) {
        core0BackgroundQueue = new DispatchQueue("Core0 Background Queue", 1, 10000, 0, 0);
    }
    return core0BackgroundQueue;
}

auto DispatchGetUiQueue() -> DispatchQueue * {
    if (uiQueue == nullptr) {
        uiQueue = new DispatchQueue("UI Queue", 1, 10000, 10, 1);
    }
    return uiQueue;
}
