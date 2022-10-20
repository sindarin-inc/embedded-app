// #include <cassert>
// #include <condition_variable>
// #include <cstdint>
// #include <cstdio>
// #include <freertos/FreeRTOS.h>
// #include <freertos/event_groups.h>
// #include <freertos/semphr.h>
// #include <freertos/task.h>
// #include <functional>
// #include <mutex>
// #include <queue>
// #include <string>
// #include <thread>
// #include <vector>
#include "Dispatch.hpp"

// Definitions for dispatch event flags
#define DISPATCH_WAKE_EVT (0x1)
#define DISPATCH_EXIT_EVT (0x2)

static dispatch_queue *Core0BackgroundQueue = nullptr;
static dispatch_queue *UIQueue = nullptr;

dispatch_queue *dispatch_get_core0_background_queue() {
    if (Core0BackgroundQueue == nullptr) {
        Core0BackgroundQueue = new dispatch_queue("Core0 Background Queue", 1, 10000, 0, 0);
    }
    return Core0BackgroundQueue;
}

dispatch_queue *dispatch_get_ui_queue() {
    if (UIQueue == nullptr) {
        UIQueue = new dispatch_queue("UI Queue", 1, 10000, 10, 1);
    }
    return UIQueue;
}

/// This Bounce implementation is pulled from bounce.cpp
template <class T, class Method, Method m, class... Params>
static auto bounce(void *priv, Params... params)
    -> decltype(((*reinterpret_cast<T *>(priv)).*m)(params...)) {
    return ((*reinterpret_cast<T *>(priv)).*m)(params...);
}

/// Convenience macro to simplify bounce statement usage
#define BOUNCE(c, m) bounce<c, decltype(&c::m), &c::m>

#pragma mark - Dispatch Class -

#pragma mark - Implementation -

dispatch_queue::dispatch_queue(std::string name, size_t thread_cnt, size_t thread_stack_size,
                               UBaseType_t priority, BaseType_t core)
    : name_{std::move(name)}, threads_(thread_cnt) {
    // Create the Mutex
    mutex_ = xSemaphoreCreateRecursiveMutex();
    assert(mutex_ != NULL && "Failed to create mutex!");

    // Create the event flags
    notify_flags_ = xEventGroupCreate();
    assert(notify_flags_ != NULL && "Failed to create event group!");

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
            reinterpret_cast<void (*)(void *)>(BOUNCE(dispatch_queue, dispatch_thread_handler)),
            threads_[i].name.c_str(), thread_stack_size, reinterpret_cast<void *>(this), priority,
            &threads_[i].thread, core);
        assert(status == pdPASS && "Failed to create thread!");
    }
}

dispatch_queue::~dispatch_queue() {
    // Signal to dispatch threads that it's time to wrap up
    quit_ = true;

    // We will join each thread to confirm exiting
    for (size_t i = 0; i < threads_.size(); ++i) {
        eTaskState state;

        do {
            // Signal wake - check exit flag
            xEventGroupSetBits(notify_flags_, DISPATCH_WAKE_EVT);

            // Wait until a thread signals exit. Timeout is acceptable.
            xEventGroupWaitBits(notify_flags_, DISPATCH_EXIT_EVT, pdTRUE, pdFALSE, 10);

            // If it was not thread_[i], that is ok, but we will loop around
            // until threads_[i] has exited
            state = eTaskGetState(threads_[i].thread);
        } while (state != eDeleted);

        threads_[i].name.clear();
    }

    // Cleanup event flags and mutex
    vEventGroupDelete(notify_flags_);

    vSemaphoreDelete(mutex_);
}

void dispatch_queue::dispatch(const fp_t &op) {
    BaseType_t status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    assert(status == pdTRUE && "Failed to lock mutex!");

    q_.push(op);

    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Notifies threads that new work has been added to the queue
    xEventGroupSetBits(notify_flags_, DISPATCH_WAKE_EVT);
}

void dispatch_queue::dispatch(fp_t &&op) {
    BaseType_t status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    assert(status == pdTRUE && "Failed to lock mutex!");

    q_.push(std::move(op));

    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Notifies threads that new work has been added to the queue
    xEventGroupSetBits(notify_flags_, DISPATCH_WAKE_EVT);
}

void dispatch_queue::dispatch_thread_handler(void) {
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
            xEventGroupWaitBits(notify_flags_, DISPATCH_WAKE_EVT, pdTRUE, pdFALSE, portMAX_DELAY);

            status = xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
            assert(status == pdTRUE && "Failed to lock mutex!");
        }
    } while (!quit_);

    // We were holding the mutex after we woke up
    status = xSemaphoreGiveRecursive(mutex_);
    assert(status == pdTRUE && "Failed to unlock mutex!");

    // Set a signal to indicate a thread exited
    status = xEventGroupSetBits(notify_flags_, DISPATCH_EXIT_EVT);
    assert(status == pdTRUE && "Failed to set event flags!");

    // Delete the current thread
    vTaskDelete(NULL);
}
